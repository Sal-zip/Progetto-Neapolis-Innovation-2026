#include "esp8266_transport.h"

#include "app_config.h"
#include "chprintf.h"

#include <string.h>

#define ESP_RESPONSE_MAX 384U

/* Mantiene la diagnostica del modem su USART2, separata dal flusso MQTT. */
static void debug_log(esp8266_transport_t *transport, const char *message) {
  if (transport->debug != NULL) {
    chprintf(transport->debug, "ESP: %s\r\n", message);
  }
}

/* Mostra soltanto il codice diagnostico emesso dal bridge, senza rischiare di
   copiare sulla console l'eco del comando CWJAP contenente la password. */
static void debug_wifi_status(esp8266_transport_t *transport,
                              const char *response) {
  const char *marker = strstr(response, "+CWJAP:STATUS,");
  if ((marker == NULL) || (transport->debug == NULL)) {
    return;
  }

  char line[96];
  size_t length = 0U;
  while ((marker[length] != '\0') && (marker[length] != '\r') &&
         (marker[length] != '\n') && (length + 1U < sizeof(line))) {
    line[length] = marker[length];
    ++length;
  }
  line[length] = '\0';
  debug_log(transport, line);
}

static void drain_serial(SerialDriver *serial) {
  /* Scarta messaggi di avvio e risposte tardive prima di una nuova transazione
     AT. Non viene mai usata mentre devono essere conservati dati MQTT. */
  uint8_t discarded[32];
  while (sdReadTimeout(serial, discarded, sizeof(discarded),
                       TIME_MS2I(20)) > 0U) {
  }
}

static bool wait_for_response(esp8266_transport_t *transport,
                              const char *success,
                              const char *alternative,
                              sysinterval_t timeout) {
  char response[ESP_RESPONSE_MAX];
  size_t used = 0U;
  const systime_t start = chVTGetSystemTimeX();

  /* Le risposte AT sono testuali durante la configurazione. Basta un buffer
     scorrevole, perche si cercano soltanto brevi indicatori di esito. */
  response[0] = '\0';
  while (chVTTimeElapsedSinceX(start) < timeout) {
    uint8_t byte;
    if (sdReadTimeout(transport->serial, &byte, 1U, TIME_MS2I(50)) == 0U) {
      continue;
    }

    /* Conserva la meta piu recente quando una risposta riempie il buffer. */
    if (used + 1U >= sizeof(response)) {
      const size_t keep = sizeof(response) / 2U;
      memmove(response, response + used - keep, keep);
      used = keep;
    }
    response[used++] = (char)byte;
    response[used] = '\0';

    if ((strstr(response, success) != NULL) ||
        ((alternative != NULL) && (strstr(response, alternative) != NULL))) {
      return true;
    }
    if ((strstr(response, "ERROR") != NULL) ||
        (strstr(response, "FAIL") != NULL)) {
      debug_wifi_status(transport, response);
      return false;
    }
  }
  debug_wifi_status(transport, response);
  debug_log(transport, "AT response timeout");
  return false;
}

static bool send_command(esp8266_transport_t *transport, const char *command,
                         const char *success, const char *alternative,
                         sysinterval_t timeout) {
  /* Ogni comando parte da un confine noto e termina soltanto alla ricezione
     dell'indicatore atteso o di un errore definitivo. */
  drain_serial(transport->serial);
  sdWrite(transport->serial, (const uint8_t *)command, strlen(command));
  return wait_for_response(transport, success, alternative, timeout);
}

static void leave_transparent_mode(esp8266_transport_t *transport) {
  /* Forza l'uscita anche dopo il reset della sola STM32: l'ESP potrebbe essere
     ancora trasparente nonostante lo stato locale sia stato reinizializzato. */
  chThdSleepMilliseconds(1100);
  sdWrite(transport->serial, (const uint8_t *)"+++", 3U);
  chThdSleepMilliseconds(1100);
  transport->transparent_mode = false;
  drain_serial(transport->serial);
}

void esp8266_transport_object_init(esp8266_transport_t *transport,
                                   SerialDriver *serial,
                                   BaseSequentialStream *debug) {
  transport->serial = serial;
  transport->debug = debug;
  transport->transparent_mode = false;
}

bool esp8266_transport_connect(esp8266_transport_t *transport) {
  char command[192];

  /* Fase 1: recupera il canale comandi anche dopo il reset della sola STM32. */
  leave_transparent_mode(transport);
  if (!send_command(transport, "AT\r\n", "OK", NULL, TIME_MS2I(1500))) {
    debug_log(transport, "no response to AT");
    return false;
  }
  /* L'eco e opzionale: un errore qui non impedisce la configurazione restante. */
  (void)send_command(transport, "ATE0\r\n", "OK", NULL, TIME_MS2I(1000));

  /* Fase 2: collega l'ESP alla rete Wi-Fi configurata, come stazione. */
  if (!send_command(transport, "AT+CWMODE=1\r\n", "OK", NULL,
                    TIME_MS2I(2000))) {
    debug_log(transport, "station mode failed");
    return false;
  }

  chsnprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"\r\n",
             APP_WIFI_SSID, APP_WIFI_PASSWORD);
  /* Il bridge attende fino a 30 s: il chiamante deve avere margine sufficiente
     per ricevere anche la risposta conclusiva e non scadere nello stesso tick. */
  if (!send_command(transport, command, "OK", NULL, TIME_S2I(40))) {
    debug_log(transport, "Wi-Fi association failed");
    return false;
  }

  /* Fase 3: un solo socket TCP e passaggio trasparente fra UART e socket. */
  if (!send_command(transport, "AT+CIPMUX=0\r\n", "OK", NULL,
                    TIME_MS2I(1500)) ||
      !send_command(transport, "AT+CIPMODE=1\r\n", "OK", NULL,
                    TIME_MS2I(1500))) {
    debug_log(transport, "TCP transparent mode setup failed");
    return false;
  }

  /* Fase 4: apre la connessione TCP non cifrata verso Mosquitto. */
  chsnprintf(command, sizeof(command),
             "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
             APP_MQTT_BROKER_HOST, (unsigned)APP_MQTT_BROKER_PORT);
  if (!send_command(transport, command, "OK", "ALREADY CONNECTED",
                    TIME_S2I(15))) {
    debug_log(transport, "broker TCP connection failed");
    return false;
  }

  /* Il prompt '>' indica che da quel momento la UART trasporta MQTT binario. */
  if (!send_command(transport, "AT+CIPSEND\r\n", ">", NULL,
                    TIME_MS2I(3000))) {
    debug_log(transport, "transparent stream start failed");
    return false;
  }

  transport->transparent_mode = true;
  debug_log(transport, "transparent TCP stream ready");
  return true;
}

void esp8266_transport_disconnect(esp8266_transport_t *transport) {
  /* Gli errori MQTT vengono recuperati ricostruendo gli stati TCP e MQTT. */
  leave_transparent_mode(transport);
  (void)send_command(transport, "AT+CIPCLOSE\r\n", "OK", "ERROR",
                     TIME_MS2I(1500));
}

bool esp8266_transport_write(esp8266_transport_t *transport,
                             const uint8_t *data, size_t length) {
  if (!transport->transparent_mode || (data == NULL)) {
    return false;
  }
  /* sdWrite e usata direttamente: la modalita trasparente non deve aggiungere
     terminatori di riga ne ricodificare i pacchetti MQTT binari. */
  return sdWrite(transport->serial, data, length) == length;
}

size_t esp8266_transport_read(esp8266_transport_t *transport,
                              uint8_t *data, size_t length,
                              sysinterval_t timeout) {
  if (!transport->transparent_mode || (data == NULL)) {
    return 0U;
  }
  return sdReadTimeout(transport->serial, data, length, timeout);
}

