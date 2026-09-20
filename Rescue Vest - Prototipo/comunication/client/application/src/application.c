#include "application.h"

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "app_config.h"
#include "esp8266_transport.h"
#include "gesture_link.h"
#include "mqtt_client.h"
#include "telemetry.h"
#include "telemetry_json.h"
#include "thread_gps.h"
#include "thread_mq2.h"
#include "thread_ppg.h"

#include <stddef.h>
#include <string.h>

#define APPLICATION_EVENT_OUTBOX_CAPACITY 8U

static BaseSequentialStream *const debug_stream =
    (BaseSequentialStream *)&SD2;

static const SerialConfig application_serial_config = {
    APP_UART_BAUD,
    0U,
    USART_CR2_STOP1_BITS,
    0U
};

static esp8266_transport_t esp_transport;
static mqtt_client_t mqtt_client;

/*===========================================================================*/
/* Sequenza applicativa.                                                     */
/*===========================================================================*/

static mutex_t sequence_mutex;
static uint32_t next_sequence;

/*===========================================================================*/
/* Outbox degli eventi critici.                                              */
/*===========================================================================*/

static application_message_t event_slots[
    APPLICATION_EVENT_OUTBOX_CAPACITY];

static mailbox_t free_event_mailbox;
static mailbox_t ready_event_mailbox;

static msg_t free_event_queue[
    APPLICATION_EVENT_OUTBOX_CAPACITY];

static msg_t ready_event_queue[
    APPLICATION_EVENT_OUTBOX_CAPACITY];

/*===========================================================================*/
/* Cache dell'ultima telemetria.                                             */
/*===========================================================================*/

static mutex_t telemetry_cache_mutex;
static application_message_t latest_telemetry;
static uint32_t telemetry_generation;

/*===========================================================================*/
/* Stato degli allarmi.                                                      */
/*===========================================================================*/

static bool ppg_emergency_latched;
static bool mq2_emergency_latched;

/*===========================================================================*/
/* Inizializzazione seriali.                                                 */
/*===========================================================================*/

static void serial_ports_init(void) {
  /* ESP8266: USART3, PC10 TX e PC11 RX. */
  palSetPadMode(GPIOC, 10U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOC, 11U, PAL_MODE_ALTERNATE(7));
  sdStart(&SD3, &application_serial_config);

  /* Debug ST-LINK: USART2, PA2 TX e PA3 RX. */
  palSetPadMode(GPIOA, 2U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3U, PAL_MODE_ALTERNATE(7));
  sdStart(&SD2, &application_serial_config);
}

/*===========================================================================*/
/* Generatore di sequenze.                                                   */
/*===========================================================================*/

static uint32_t sequence_take(void) {
  uint32_t sequence;

  chMtxLock(&sequence_mutex);

  sequence = next_sequence++;

  if (next_sequence == 0U) {
    next_sequence = 1U;
  }

  chMtxUnlock(&sequence_mutex);

  return sequence;
}

/*===========================================================================*/
/* Outbox eventi critici.                                                    */
/*===========================================================================*/

static void event_outbox_init(void) {
  chMBObjectInit(
      &free_event_mailbox,
      free_event_queue,
      APPLICATION_EVENT_OUTBOX_CAPACITY);

  chMBObjectInit(
      &ready_event_mailbox,
      ready_event_queue,
      APPLICATION_EVENT_OUTBOX_CAPACITY);

  for (size_t i = 0U;
       i < APPLICATION_EVENT_OUTBOX_CAPACITY;
       ++i) {
    (void)chMBPostTimeout(
        &free_event_mailbox,
        (msg_t)i,
        TIME_IMMEDIATE);
  }
}

static bool event_outbox_enqueue(
    const application_message_t *message) {
  if (message == NULL) {
    return false;
  }

  msg_t index;

  if (chMBFetchTimeout(
          &free_event_mailbox,
          &index,
          TIME_IMMEDIATE) != MSG_OK) {
    return false;
  }

  event_slots[(size_t)index] = *message;

  if (chMBPostTimeout(
          &ready_event_mailbox,
          index,
          TIME_IMMEDIATE) != MSG_OK) {
    (void)chMBPostTimeout(
        &free_event_mailbox,
        index,
        TIME_IMMEDIATE);

    return false;
  }

  return true;
}

static application_message_t *event_outbox_take(void) {
  msg_t index;

  if (chMBFetchTimeout(
          &ready_event_mailbox,
          &index,
          TIME_IMMEDIATE) != MSG_OK) {
    return NULL;
  }

  return &event_slots[(size_t)index];
}

static void event_outbox_release(
    application_message_t *message) {
  if ((message == NULL) ||
      (message < event_slots) ||
      (message >=
       event_slots + APPLICATION_EVENT_OUTBOX_CAPACITY)) {
    return;
  }

  const msg_t index =
      (msg_t)(message - event_slots);

  (void)chMBPostTimeout(
      &free_event_mailbox,
      index,
      TIME_INFINITE);
}

/*===========================================================================*/
/* Telemetry cache.                                                          */
/*===========================================================================*/

static void telemetry_cache_store(
    const application_message_t *message) {
  if (message == NULL) {
    return;
  }

  chMtxLock(&telemetry_cache_mutex);

  latest_telemetry = *message;
  ++telemetry_generation;

  if (telemetry_generation == 0U) {
    telemetry_generation = 1U;
  }

  chMtxUnlock(&telemetry_cache_mutex);
}

static bool telemetry_cache_copy_if_new(
    uint32_t last_generation,
    application_message_t *message,
    uint32_t *generation) {
  if ((message == NULL) || (generation == NULL)) {
    return false;
  }

  bool available = false;

  chMtxLock(&telemetry_cache_mutex);

  if ((telemetry_generation != 0U) &&
      (telemetry_generation != last_generation)) {
    *message = latest_telemetry;
    *generation = telemetry_generation;
    available = true;
  }

  chMtxUnlock(&telemetry_cache_mutex);

  return available;
}

/*===========================================================================*/
/* Costruzione e accodamento eventi.                                         */
/*===========================================================================*/

static bool enqueue_event(
    application_message_kind_t kind,
    const telemetry_snapshot_t *snapshot) {
  application_message_t message;

  if (!telemetry_json_build(
          kind,
          sequence_take(),
          snapshot,
          &message)) {
    return false;
  }

  return event_outbox_enqueue(&message);
}

/*===========================================================================*/
/* Callback GestureLink.                                                     */
/*===========================================================================*/

void application_on_gesture(gesture_command_t command) {
  telemetry_snapshot_t snapshot;

  telemetry_capture(&snapshot);

  switch (command) {
    case GESTURE_COMMAND_ACTIVATE:
      if (!enqueue_event(
              APPLICATION_MESSAGE_ACTIVATED,
              &snapshot)) {
        chprintf(
            debug_stream,
            "APP: unable to queue activation event\r\n");
      }
      break;

    case GESTURE_COMMAND_SOS:
      if (!enqueue_event(
              APPLICATION_MESSAGE_MANUAL_SOS,
              &snapshot)) {
        chprintf(
            debug_stream,
            "APP: unable to queue manual SOS\r\n");
      }
      break;

    case GESTURE_COMMAND_SAFE_ZONE:
      if (!enqueue_event(
              APPLICATION_MESSAGE_ZONE_SAFE,
              &snapshot)) {
        chprintf(
            debug_stream,
            "APP: unable to queue safe-zone event\r\n");
      }
      break;

    default:
      chprintf(
          debug_stream,
          "APP: unknown gesture command\r\n");
      break;
  }
}

/*===========================================================================*/
/* Thread sensori e telemetria.                                              */
/*===========================================================================*/

static THD_WORKING_AREA(
    wa_telemetry,
    1536);

static THD_FUNCTION(
    telemetry_thread,
    argument) {
  (void)argument;

  chRegSetThreadName("telemetry");

  while (true) {
    telemetry_snapshot_t snapshot;
    application_message_t telemetry_message;

    telemetry_capture(&snapshot);

    if (telemetry_json_build(
            APPLICATION_MESSAGE_TELEMETRY,
            sequence_take(),
            &snapshot,
            &telemetry_message)) {
      /*
       * La telemetria periodica non occupa la coda degli SOS.
       * Se il broker è offline viene conservato soltanto il campione
       * più recente.
       */
      telemetry_cache_store(&telemetry_message);
    }

    /*
     * L'allarme viene generato soltanto sul fronte di salita.
     * Una condizione persistente non riempie ripetutamente l'outbox.
     */
    if (snapshot.ppg_fresh && snapshot.ppg.valid) {
      if (snapshot.ppg.emergency &&
          !ppg_emergency_latched) {
        if (!enqueue_event(
                APPLICATION_MESSAGE_PPG_SOS,
                &snapshot)) {
          chprintf(
              debug_stream,
              "APP: PPG SOS outbox full\r\n");
        }
      }

      ppg_emergency_latched =
          snapshot.ppg.emergency;
    }

    if (snapshot.mq2_fresh && snapshot.mq2.valid) {
      if (snapshot.mq2.emergency &&
          !mq2_emergency_latched) {
        if (!enqueue_event(
                APPLICATION_MESSAGE_MQ2_SOS,
                &snapshot)) {
          chprintf(
              debug_stream,
              "APP: MQ2 SOS outbox full\r\n");
        }
      }

      mq2_emergency_latched =
          snapshot.mq2.emergency;
    }

    chThdSleepMilliseconds(
        APP_TELEMETRY_PERIOD_MS);
  }
}

/*===========================================================================*/
/* Thread MQTT.                                                              */
/*===========================================================================*/

static THD_WORKING_AREA(
    wa_mqtt,
    3072);

static THD_FUNCTION(
    mqtt_thread,
    argument) {
  (void)argument;

  chRegSetThreadName("mqtt-client");

  application_message_t *pending_event = NULL;
  application_message_t pending_telemetry;

  bool telemetry_in_flight = false;
  uint32_t telemetry_in_flight_generation = 0U;
  uint32_t telemetry_sent_generation = 0U;

  chThdSleepMilliseconds(2000);

  while (true) {
    chprintf(
        debug_stream,
        "MQTT: connecting to %s:%u\r\n",
        APP_MQTT_BROKER_HOST,
        (unsigned)APP_MQTT_BROKER_PORT);

    if (esp8266_transport_connect(&esp_transport) &&
        mqtt_client_open(&mqtt_client)) {
      chprintf(
          debug_stream,
          "MQTT: connected as %s\r\n",
          APP_MQTT_CLIENT_ID);

      while (true) {
        const application_message_t *message = NULL;

        /*
         * Gli eventi critici hanno precedenza sulla telemetria.
         * Un messaggio già in attesa di PUBACK non viene sostituito.
         */
        if ((pending_event == NULL) &&
            !telemetry_in_flight) {
          pending_event = event_outbox_take();
        }

        if (pending_event != NULL) {
          message = pending_event;
        }
        else if (telemetry_in_flight) {
          message = &pending_telemetry;
        }
        else if (telemetry_cache_copy_if_new(
                     telemetry_sent_generation,
                     &pending_telemetry,
                     &telemetry_in_flight_generation)) {
          telemetry_in_flight = true;
          message = &pending_telemetry;
        }

        if (message != NULL) {
          chprintf(
              debug_stream,
              "MQTT: publishing eventId=%s topic=%s\r\n",
              message->event_id,
              message->topic);

          if (!mqtt_client_publish_qos1(
                  &mqtt_client,
                  message->topic,
                  message->payload,
                  message->payload_length)) {
            chprintf(
                debug_stream,
                "MQTT: PUBACK missing; payload retained\r\n");
            break;
          }

          chprintf(
              debug_stream,
              "MQTT: acknowledged eventId=%s\r\n",
              message->event_id);

          if (pending_event != NULL) {
            event_outbox_release(pending_event);
            pending_event = NULL;
          }
          else {
            telemetry_sent_generation =
                telemetry_in_flight_generation;

            telemetry_in_flight = false;
          }

          continue;
        }

        if (!mqtt_client_yield(&mqtt_client)) {
          chprintf(
              debug_stream,
              "MQTT: session interrupted\r\n");
          break;
        }
      }
    }
    else {
      chprintf(
          debug_stream,
          "MQTT: connection failed\r\n");
    }

    esp8266_transport_disconnect(&esp_transport);
    chThdSleepMilliseconds(5000);
  }
}

/*===========================================================================*/
/* Avvio applicazione.                                                       */
/*===========================================================================*/

void application_start(void) {
  serial_ports_init();

  chMtxObjectInit(&sequence_mutex);
  chMtxObjectInit(&telemetry_cache_mutex);

  next_sequence = 1U;
  telemetry_generation = 0U;

  ppg_emergency_latched = false;
  mq2_emergency_latched = false;

  event_outbox_init();

  esp8266_transport_object_init(
      &esp_transport,
      &SD3,
      debug_stream);

  mqtt_client_object_init(
      &mqtt_client,
      &esp_transport);

  /*
   * Ogni funzione start inizializza il proprio driver e crea
   * esclusivamente i thread appartenenti al sensore.
   */
  gps_start();
  ppg_start();
  mq2_start();

  /*
   * Il link con la seconda board riceve i tre comandi Gesture.
   * Un errore del link non impedisce agli altri sensori di funzionare.
   */
  if (!gesture_link_start(application_on_gesture)) {
    chprintf(
        debug_stream,
        "APP: GestureLink initialization failed\r\n");
  }

  chThdCreateStatic(
      wa_telemetry,
      sizeof(wa_telemetry),
      NORMALPRIO + 1,
      telemetry_thread,
      NULL);

  chThdCreateStatic(
      wa_mqtt,
      sizeof(wa_mqtt),
      NORMALPRIO + 2,
      mqtt_thread,
      NULL);

  chprintf(
      debug_stream,
      "Neapolis 2026 integrated client started\r\n");
}
