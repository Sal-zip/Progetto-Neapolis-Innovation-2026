#include "mqtt_client.h"

#include "app_config.h"

#include <string.h>

typedef enum {
  /* Distingue la normale inattivita da un pacchetto MQTT troncato o errato. */
  MQTT_READ_ERROR = -1,
  MQTT_READ_TIMEOUT = 0,
  MQTT_READ_PACKET = 1
} mqtt_read_result_t;

static bool write_u16(uint8_t *buffer, size_t capacity, size_t *offset,
                      uint16_t value) {
  if (*offset + 2U > capacity) {
    return false;
  }
  buffer[(*offset)++] = (uint8_t)(value >> 8);
  buffer[(*offset)++] = (uint8_t)value;
  return true;
}

static bool write_utf8(uint8_t *buffer, size_t capacity, size_t *offset,
                       const char *text) {
  /* Le stringhe UTF-8 MQTT hanno lunghezza a due byte in formato big-endian. */
  const size_t length = strlen(text);
  if ((length > UINT16_MAX) ||
      !write_u16(buffer, capacity, offset, (uint16_t)length) ||
      (*offset + length > capacity)) {
    return false;
  }
  memcpy(buffer + *offset, text, length);
  *offset += length;
  return true;
}

static size_t encode_remaining_length(uint8_t output[4], size_t value) {
  /* MQTT codifica Remaining Length in base 128, con sette bit per byte. */
  size_t used = 0U;
  do {
    uint8_t byte = (uint8_t)(value % 128U);
    value /= 128U;
    if (value > 0U) {
      byte |= 0x80U;
    }
    output[used++] = byte;
  } while ((value > 0U) && (used < 4U));
  return used;
}

static bool packet_fits(size_t body_length) {
  uint8_t encoded[4];
  const size_t encoded_length = encode_remaining_length(encoded, body_length);
  return 1U + encoded_length + body_length <= MQTT_PACKET_MAX;
}

static bool send_packet(mqtt_client_t *client, uint8_t header,
                        const uint8_t *body, size_t body_length) {
  uint8_t prefix[5];
  if (!packet_fits(body_length)) {
    return false;
  }

  /* Header e corpo possono usare scritture UART separate: TCP preserva
     l'ordine e MQTT non impone una scrittura per pacchetto completo. */
  prefix[0] = header;
  const size_t encoded = encode_remaining_length(prefix + 1U, body_length);
  return esp8266_transport_write(client->transport, prefix, encoded + 1U) &&
         ((body_length == 0U) ||
          esp8266_transport_write(client->transport, body, body_length));
}

static mqtt_read_result_t read_packet(mqtt_client_t *client,
                                      sysinterval_t first_byte_timeout,
                                      size_t *packet_length) {
  size_t used = 0U;
  uint8_t byte;

  /* Un timeout prima del primo byte rappresenta normale inattivita. */
  if (esp8266_transport_read(client->transport, &byte, 1U,
                             first_byte_timeout) == 0U) {
    return MQTT_READ_TIMEOUT;
  }
  client->packet[used++] = byte;

  size_t remaining = 0U;
  size_t multiplier = 1U;
  unsigned encoded_bytes = 0U;
  /* Dopo l'inizio del pacchetto, non completarlo e un errore di trasporto. */
  do {
    if ((encoded_bytes >= 4U) ||
        (esp8266_transport_read(client->transport, &byte, 1U,
                                TIME_MS2I(1000)) == 0U)) {
      return MQTT_READ_ERROR;
    }
    client->packet[used++] = byte;
    remaining += (size_t)(byte & 0x7FU) * multiplier;
    multiplier *= 128U;
    ++encoded_bytes;
  } while ((byte & 0x80U) != 0U);

  if (remaining > sizeof(client->packet) - used) {
    return MQTT_READ_ERROR;
  }

  size_t received = 0U;
  while (received < remaining) {
    const size_t count = esp8266_transport_read(
        client->transport, client->packet + used + received,
        remaining - received, TIME_MS2I(1000));
    if (count == 0U) {
      return MQTT_READ_ERROR;
    }
    received += count;
  }

  *packet_length = used + remaining;
  return MQTT_READ_PACKET;
}

static size_t variable_header_offset(const uint8_t *packet,
                                     size_t packet_length) {
  size_t offset = 1U;
  while ((offset < packet_length) && ((packet[offset++] & 0x80U) != 0U)) {
  }
  return offset;
}

static bool process_puback(mqtt_client_t *client, size_t packet_length) {
  const size_t offset = variable_header_offset(client->packet, packet_length);
  if ((client->packet[0] != 0x40U) || (offset + 2U != packet_length)) {
    return false;
  }

  const uint16_t packet_id = ((uint16_t)client->packet[offset] << 8) |
                             client->packet[offset + 1U];
  if (client->publish_pending &&
      (packet_id == client->pending_publish_id)) {
    client->publish_pending = false;
  }
  /* Un PUBACK valido ma riferito a un ID precedente non corrompe lo stato. */
  return true;
}

static bool process_packet(mqtt_client_t *client, size_t packet_length) {
  const uint8_t type = client->packet[0] >> 4;
  client->idle_seconds = 0U;

  switch (type) {
  case 4U:
    return process_puback(client, packet_length);
  case 13U: { /* PINGRESP. */
    const size_t offset = variable_header_offset(client->packet,
                                                 packet_length);
    if ((client->packet[0] != 0xD0U) || (offset != packet_length)) {
      return false;
    }
    client->ping_outstanding = false;
    return true;
  }
  default:
    /* Il client publisher non ha sottoscrizioni e non attende altri pacchetti. */
    return true;
  }
}

static bool send_connect(mqtt_client_t *client) {
  uint8_t body[384];
  size_t used = 0U;
  /* Clean Session resta zero, come nel server, e usa un Client ID stabile. */
  uint8_t flags = 0U;

  /* MQTT 3.1.1 non consente il flag Password senza il flag User Name. */
  if ((APP_MQTT_PASSWORD[0] != '\0') && (APP_MQTT_USERNAME[0] == '\0')) {
    return false;
  }
  if (APP_MQTT_USERNAME[0] != '\0') {
    flags |= 0x80U;
  }
  if (APP_MQTT_PASSWORD[0] != '\0') {
    flags |= 0x40U;
  }

  if (!write_utf8(body, sizeof(body), &used, "MQTT") ||
      (used + 4U > sizeof(body))) {
    return false;
  }
  body[used++] = 4U;
  body[used++] = flags;
  body[used++] = (uint8_t)(APP_MQTT_KEEP_ALIVE_SEC >> 8);
  body[used++] = (uint8_t)APP_MQTT_KEEP_ALIVE_SEC;

  if (!write_utf8(body, sizeof(body), &used, APP_MQTT_CLIENT_ID) ||
      ((flags & 0x80U) &&
       !write_utf8(body, sizeof(body), &used, APP_MQTT_USERNAME)) ||
      ((flags & 0x40U) &&
       !write_utf8(body, sizeof(body), &used, APP_MQTT_PASSWORD))) {
    return false;
  }
  return send_packet(client, 0x10U, body, used);
}

static bool wait_for_connack(mqtt_client_t *client) {
  size_t length;
  if (read_packet(client, TIME_S2I(5), &length) != MQTT_READ_PACKET) {
    return false;
  }
  const size_t offset = variable_header_offset(client->packet, length);
  return (client->packet[0] == 0x20U) && (offset + 2U == length) &&
         ((client->packet[offset] & 0xFEU) == 0U) &&
         (client->packet[offset + 1U] == 0U);
}

static uint16_t take_next_packet_id(mqtt_client_t *client) {
  uint16_t packet_id = client->next_packet_id++;
  /* Lo zero non e un Packet Identifier MQTT valido. */
  if (client->next_packet_id == 0U) {
    client->next_packet_id = 1U;
  }
  if (packet_id == 0U) {
    packet_id = client->next_packet_id++;
  }
  return packet_id;
}

static bool wait_for_puback(mqtt_client_t *client, uint16_t packet_id) {
  const systime_t start = chVTGetSystemTimeX();
  while (chVTTimeElapsedSinceX(start) < TIME_S2I(5)) {
    size_t length;
    const mqtt_read_result_t result =
        read_packet(client, TIME_MS2I(500), &length);
    if (result == MQTT_READ_TIMEOUT) {
      continue;
    }
    if ((result == MQTT_READ_ERROR) || !process_packet(client, length)) {
      return false;
    }
    if (!client->publish_pending) {
      return true;
    }
  }

  /* Il chiamante riconnettera e ritentera lo stesso messaggio con DUP. */
  (void)packet_id;
  return false;
}

void mqtt_client_object_init(mqtt_client_t *client,
                             esp8266_transport_t *transport) {
  client->transport = transport;
  client->next_packet_id = 1U;
  client->pending_publish_id = 0U;
  client->idle_seconds = 0U;
  client->ping_outstanding = false;
  client->publish_pending = false;
}

bool mqtt_client_open(mqtt_client_t *client) {
  /* Lo stato keep-alive appartiene alla connessione TCP appena ricostruita. */
  client->idle_seconds = 0U;
  client->ping_outstanding = false;
  return send_connect(client) && wait_for_connack(client);
}

bool mqtt_client_publish_qos1(mqtt_client_t *client,
                              const char *topic,
                              const uint8_t *payload,
                              size_t payload_length) {
  if ((client == NULL) || (topic == NULL) || (topic[0] == '\0') ||
      ((payload == NULL) && (payload_length > 0U))) {
    return false;
  }

  const bool duplicate = client->publish_pending;
  const uint16_t packet_id = duplicate ? client->pending_publish_id
                                       : take_next_packet_id(client);
  size_t used = 0U;

  if (!write_utf8(client->packet, sizeof(client->packet), &used, topic) ||
      !write_u16(client->packet, sizeof(client->packet), &used, packet_id) ||
      (payload_length > sizeof(client->packet) - used) ||
      !packet_fits(used + payload_length)) {
    return false;
  }
  if (payload_length > 0U) {
    memcpy(client->packet + used, payload, payload_length);
    used += payload_length;
  }

  if (!duplicate) {
    client->pending_publish_id = packet_id;
    client->publish_pending = true;
  }

  /* 0x32 = PUBLISH QoS 1; bit 3 aggiunge DUP; RETAIN resta sempre zero. */
  const uint8_t header = (uint8_t)(0x32U | (duplicate ? 0x08U : 0U));
  client->idle_seconds = 0U;
  if (!send_packet(client, header, client->packet, used)) {
    return false;
  }
  return wait_for_puback(client, packet_id);
}

bool mqtt_client_publish_is_pending(const mqtt_client_t *client) {
  return (client != NULL) && client->publish_pending;
}

bool mqtt_client_yield(mqtt_client_t *client) {
  size_t length;
  const mqtt_read_result_t result =
      read_packet(client, TIME_S2I(1), &length);
  if (result == MQTT_READ_ERROR) {
    return false;
  }
  if (result == MQTT_READ_PACKET) {
    return process_packet(client, length);
  }

  ++client->idle_seconds;
  const uint16_t ping_interval = APP_MQTT_KEEP_ALIVE_SEC / 2U;
  if (client->idle_seconds < ping_interval) {
    return true;
  }
  client->idle_seconds = 0U;
  if (client->ping_outstanding) {
    return false;
  }
  client->ping_outstanding = true;
  return send_packet(client, 0xC0U, NULL, 0U);
}
