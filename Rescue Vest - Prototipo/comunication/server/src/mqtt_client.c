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

static bool send_packet(mqtt_client_t *client, uint8_t header,
                        const uint8_t *body, size_t body_length) {
  uint8_t prefix[5];
  /* Header e corpo possono usare scritture UART separate: TCP preserva
     l'ordine e MQTT non impone una scrittura per ogni pacchetto completo. */
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

  /* Un ingresso troppo grande viene rifiutato prima di superare il buffer. */
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

static bool send_puback(mqtt_client_t *client, uint16_t packet_id) {
  /* PUBACK ha un corpo fisso di due byte contenente l'ID del PUBLISH. */
  uint8_t body[2] = {(uint8_t)(packet_id >> 8), (uint8_t)packet_id};
  return send_packet(client, 0x40U, body, sizeof(body));
}

static bool process_publish(mqtt_client_t *client, size_t packet_length) {
  const uint8_t header = client->packet[0];
  const unsigned qos = (header >> 1) & 0x03U;
  size_t offset = variable_header_offset(client->packet, packet_length);

  /* La sottoscrizione richiede QoS massimo 1; QoS 2 resta fuori ambito. */
  if ((qos > 1U) || (offset + 2U > packet_length)) {
    return false;
  }

  const size_t topic_length = ((size_t)client->packet[offset] << 8) |
                              client->packet[offset + 1U];
  offset += 2U;
  if ((topic_length == 0U) || (topic_length >= 128U) ||
      (offset + topic_length > packet_length)) {
    return false;
  }

  char topic[128];
  memcpy(topic, client->packet + offset, topic_length);
  topic[topic_length] = '\0';
  offset += topic_length;

  uint16_t packet_id = 0U;
  if (qos == 1U) {
    if (offset + 2U > packet_length) {
      return false;
    }
    packet_id = ((uint16_t)client->packet[offset] << 8) |
                client->packet[offset + 1U];
    offset += 2U;
  }

  /* La callback copia la richiesta fuori dal buffer MQTT riutilizzabile.
     Un esito false sopprime PUBACK per consentire la riconsegna QoS 1. */
  const bool accepted = (client->on_publish != NULL) &&
      client->on_publish(topic, client->packet + offset,
                         packet_length - offset, (header & 0x01U) != 0U,
                         (header & 0x08U) != 0U);
  if (!accepted) {
    return false;
  }
  return (qos == 0U) || send_puback(client, packet_id);
}

static bool process_packet(mqtt_client_t *client, size_t packet_length) {
  const uint8_t type = client->packet[0] >> 4;
  client->idle_seconds = 0U;

  /* I tipi non necessari in questo incremento di ricezione vengono ignorati
     dopo la validazione; le future pubblicazioni useranno qui PUBACK. */
  switch (type) {
  case 3U:
    return process_publish(client, packet_length);
  case 4U: /* PUBACK per le future pubblicazioni del server. */
    return true;
  case 9U: /* Conferma della sottoscrizione. */
    return true;
  case 13U: /* Risposta al controllo di presenza. */
    client->ping_outstanding = false;
    return true;
  default:
    return true;
  }
}

static bool send_connect(mqtt_client_t *client) {
  uint8_t body[384];
  size_t used = 0U;
  /* Clean Session resta zero e lega il traffico QoS accodato al valore stabile
     APP_MQTT_CLIENT_ID registrato sul broker Mosquitto. */
  uint8_t flags = 0U;

  if (APP_MQTT_USERNAME[0] != '\0') {
    flags |= 0x80U;
  }
  if (APP_MQTT_PASSWORD[0] != '\0') {
    flags |= 0x40U;
  }

  /* Header variabile: nome protocollo, livello MQTT 3.1.1, flag, keep-alive. */
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
  /* Solo il codice di ritorno zero rappresenta un CONNACK riuscito. */
  return ((client->packet[0] >> 4) == 2U) && (offset + 2U == length) &&
         ((client->packet[offset] & 0xFEU) == 0U) &&
         (client->packet[offset + 1U] == 0U);
}

static bool send_subscribe(mqtt_client_t *client, uint16_t packet_id) {
  uint8_t body[256];
  size_t used = 0U;
  const char *const topics[APP_MQTT_TOPIC_COUNT] = {
      APP_MQTT_SOS_TOPIC,
      APP_MQTT_TELEMETRY_TOPIC,
      APP_MQTT_STATUS_TOPIC
  };
  if (!write_u16(body, sizeof(body), &used, packet_id)) {
    return false;
  }
  /* Un singolo SUBSCRIBE registra tre filtri esatti con QoS 1. */
  for (size_t i = 0U; i < APP_MQTT_TOPIC_COUNT; ++i) {
    if (!write_utf8(body, sizeof(body), &used, topics[i]) ||
        (used >= sizeof(body))) {
      return false;
    }
    body[used++] = 1U;
  }
  return send_packet(client, 0x82U, body, used);
}

static bool wait_for_suback(mqtt_client_t *client, uint16_t packet_id) {
  const systime_t start = chVTGetSystemTimeX();
  while (chVTTimeElapsedSinceX(start) < TIME_S2I(5)) {
    size_t length;
    const mqtt_read_result_t result =
        read_packet(client, TIME_MS2I(500), &length);
    if (result == MQTT_READ_TIMEOUT) {
      continue;
    }
    if (result == MQTT_READ_ERROR) {
      return false;
    }

    const uint8_t type = client->packet[0] >> 4;
    const size_t offset = variable_header_offset(client->packet, length);
    /* Una sessione persistente puo consegnare PUBLISH accodati mentre si
       attende il nuovo SUBACK; entrambi i casi devono essere gestiti. */
    if (type == 9U) {
      if ((offset + 2U + APP_MQTT_TOPIC_COUNT != length) ||
          (client->packet[offset] != (uint8_t)(packet_id >> 8)) ||
          (client->packet[offset + 1U] != (uint8_t)packet_id)) {
        return false;
      }
      for (size_t i = 0U; i < APP_MQTT_TOPIC_COUNT; ++i) {
        if (client->packet[offset + 2U + i] == 0x80U) {
          return false;
        }
      }
      return true;
    }
    if (!process_packet(client, length)) {
      return false;
    }
  }
  return false;
}

void mqtt_client_object_init(mqtt_client_t *client,
                             esp8266_transport_t *transport,
                             mqtt_publish_callback_t callback) {
  client->transport = transport;
  client->on_publish = callback;
  client->next_packet_id = 1U;
  client->idle_seconds = 0U;
  client->ping_outstanding = false;
}

bool mqtt_client_open(mqtt_client_t *client) {
  /* TCP e gia aperto: questa funzione stabilisce soltanto la sessione MQTT. */
  if (!send_connect(client) || !wait_for_connack(client)) {
    return false;
  }

  const uint16_t packet_id = client->next_packet_id++;
  return send_subscribe(client, packet_id) &&
         wait_for_suback(client, packet_id);
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

  /* Con attese di un secondo, questo contatore approssima il tempo soltanto
     per il keep-alive e non deve essere usato come timestamp applicativo. */
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
