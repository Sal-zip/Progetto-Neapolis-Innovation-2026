#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "app_config.h"
#include "esp8266_transport.h"
#include "mqtt_client.h"
#include "mqtt_inbox.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* L'uscita diagnostica e separata fisicamente dalla USART ESP/MQTT. */
static BaseSequentialStream *const debug_stream =
    (BaseSequentialStream *)&SD2;

static const SerialConfig serial_config = {
    APP_UART_BAUD,
    0U,
    USART_CR2_STOP1_BITS,
    0U
};

/* Gli oggetti persistenti sono statici per evitare allocazioni nello heap. */
static esp8266_transport_t esp_transport;
static mqtt_client_t mqtt_client;

static void serial_init(void) {
  /* USART1, PC4 TX / PC5 RX: ESP8266 con firmware ESP-AT. */
  palSetPadMode(GPIOC, 4U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOC, 5U, PAL_MODE_ALTERNATE(7));
  sdStart(&SD1, &serial_config);

  /* USART2, PA2 TX / PA3 RX: diagnostica sulla porta COM virtuale ST-LINK. */
  palSetPadMode(GPIOA, 2U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3U, PAL_MODE_ALTERNATE(7));
  sdStart(&SD2, &serial_config);
}

static bool on_mqtt_publish(const char *topic, const uint8_t *payload,
                            size_t payload_length, bool retained,
                            bool duplicate) {
  /* Il filtro MQTT copre il namespace Neapolis; questa whitelist lascia
     entrare soltanto i tre flussi definiti dal client. */
  if ((strcmp(topic, APP_MQTT_SOS_TOPIC) != 0) &&
      (strcmp(topic, APP_MQTT_TELEMETRY_TOPIC) != 0) &&
      (strcmp(topic, APP_MQTT_STATUS_TOPIC) != 0)) {
    return true;
  }

  /* Restituire true autorizza mqtt_client.c a inviare PUBACK per QoS 1. */
  const bool queued = mqtt_inbox_try_put(topic, payload, payload_length,
                                         retained, duplicate);
  if (!queued) {
    chprintf(debug_stream,
             "MQTT queue full/invalid: message not acknowledged\r\n");
  }
  return queued;
}

static THD_WORKING_AREA(wa_mqtt, 3072);
static THD_FUNCTION(mqtt_thread, argument) {
  (void)argument;
  chRegSetThreadName("mqtt-server");

  /* Attende che alimentazione e ROM di avvio dell'ESP8266 si stabilizzino. */
  chThdSleepMilliseconds(2000);
  while (true) {
    chprintf(debug_stream, "MQTT: connecting to %s:%u\r\n",
             APP_MQTT_BROKER_HOST, (unsigned)APP_MQTT_BROKER_PORT);

    if (esp8266_transport_connect(&esp_transport) &&
        mqtt_client_open(&mqtt_client)) {
      chprintf(debug_stream,
               "MQTT: subscribed QoS1 to SOS, telemetry and status topics\r\n");
      /* mqtt_client_yield blocca al massimo un secondo e restituisce false in
         caso di pacchetto errato, keep-alive mancato o errore di trasporto. */
      while (mqtt_client_yield(&mqtt_client)) {
      }
      chprintf(debug_stream, "MQTT: session interrupted\r\n");
    } else {
      chprintf(debug_stream, "MQTT: connection/subscription failed\r\n");
    }

    esp8266_transport_disconnect(&esp_transport);
    chThdSleepMilliseconds(5000);
  }
}

static void print_safe_payload(const mqtt_message_t *message) {
  chprintf(debug_stream, "JSON payload (%u bytes): ",
           (unsigned)message->payload_length);
  /* Non usare mai il payload del broker come stringa di formato printf. */
  for (size_t i = 0U; i < message->payload_length; ++i) {
    const uint8_t byte = message->payload[i];
    chprintf(debug_stream, "%c",
             ((byte >= 0x20U) && (byte <= 0x7EU)) ? (char)byte : '.');
  }
  chprintf(debug_stream, "\r\n");
}

static THD_WORKING_AREA(wa_message_processor, 1536);
static THD_FUNCTION(message_processor_thread, argument) {
  (void)argument;
  chRegSetThreadName("message-processor");

  while (true) {
    /* Attende sulla mailbox senza consumare CPU finche non arriva un messaggio. */
    mqtt_message_t *message = mqtt_inbox_take();
    if (message == NULL) {
      continue;
    }

    chprintf(debug_stream, "MQTT received: topic=%s retained=%u duplicate=%u\r\n",
             message->topic, message->retained ? 1U : 0U,
             message->duplicate ? 1U : 0U);
    print_safe_payload(message);

    /* La dashboard sul secondo ESP inoltra il JSON al browser, che mantiene
       lo stato per operatore. Qui il confine resta deterministico e senza heap. */
    mqtt_inbox_release(message);
  }
}

int main(void) {
  /* L'HAL deve precedere il kernel RT, come richiesto da ChibiOS. */
  halInit();
  chSysInit();

  serial_init();
  mqtt_inbox_init();
  esp8266_transport_object_init(&esp_transport, &SD1, debug_stream);
  mqtt_client_object_init(&mqtt_client, &esp_transport, on_mqtt_publish);

  /* MQTT ha priorita superiore all'elaborazione, affinche conferme di rete e
     keep-alive non vengano ritardati dal lavoro applicativo. */
  chThdCreateStatic(wa_message_processor, sizeof(wa_message_processor),
                    NORMALPRIO + 1, message_processor_thread, NULL);
  chThdCreateStatic(wa_mqtt, sizeof(wa_mqtt), NORMALPRIO + 2,
                    mqtt_thread, NULL);

  chprintf(debug_stream, "Neapolis 2026 SOS MQTT server starting\r\n");
  while (true) {
    chThdSleepMilliseconds(1000);
  }
}
