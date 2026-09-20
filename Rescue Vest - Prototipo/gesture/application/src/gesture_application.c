#include "gesture_application.h"

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "buzzer.h"
#include "gesture.h"
#include "gesture_events.h"
#include "gesture_link.h"

#include <stdbool.h>

/*===========================================================================*/
/* Configurazione.                                                           */
/*===========================================================================*/

/*
 * USART2 collegata alla Virtual COM dello ST-LINK:
 *
 *   PA2 -> USART2_TX
 *   PA3 -> USART2_RX
 */
#define GESTURE_DEBUG_BAUD 38400U

/*
 * Tempo tra due tentativi di inizializzazione del collegamento con la
 * board principale.
 */
#define GESTURE_LINK_RETRY_DELAY_MS 1000U

/*
 * Stream utilizzato dai moduli gesture e application per i messaggi
 * diagnostici.
 */
static BaseSequentialStream *const debug_stream =
    (BaseSequentialStream *)&SD2;

static const SerialConfig debug_serial_config = {
    GESTURE_DEBUG_BAUD,
    0U,
    USART_CR2_STOP1_BITS,
    0U
};

/* Evita l'avvio multiplo dell'applicazione. */
static bool application_started = false;

/*===========================================================================*/
/* Inizializzazione seriale di debug.                                        */
/*===========================================================================*/

static void debug_serial_start(void) {

  /*
   * USART2:
   *
   *   PA2 = TX
   *   PA3 = RX
   *   AF7 = USART2
   */
  palSetPadMode(GPIOA, 2U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3U, PAL_MODE_ALTERNATE(7));

  sdStart(&SD2, &debug_serial_config);
}

/*===========================================================================*/
/* Invio di un comando alla board principale.                                */
/*===========================================================================*/

static void send_gesture_command(
    gesture_command_t command,
    buzzer_sound_t sound,
    const char *description) {

  /*
   * Il feedback acustico conferma immediatamente all'operatore che la
   * gesture è stata riconosciuta, indipendentemente dall'esito del link.
   */
  buzzer_play(sound);

  chprintf(
      debug_stream,
      "GESTURE: %s riconosciuta\r\n",
      description);

  /*
   * gesture_link_send() invia il frame e attende l'ACK dalla board
   * principale. Il modulo communication gestisce automaticamente:
   *
   *   - numero di sequenza;
   *   - CRC;
   *   - timeout;
   *   - ritrasmissioni;
   *   - controllo dell'ACK.
   */
  if (gesture_link_send(command)) {
    chprintf(
        debug_stream,
        "GESTURE LINK: comando %s consegnato\r\n",
        description);
  }
  else {
    chprintf(
        debug_stream,
        "GESTURE LINK: consegna %s fallita\r\n",
        description);
  }
}

/*===========================================================================*/
/* Elaborazione degli eventi gesture.                                        */
/*===========================================================================*/

static void process_gesture_events(eventflags_t flags) {

  /*
   * Doppio movimento:
   * attivazione del sistema.
   */
  if ((flags & EVT_GESTURE_DOUBLE_WAVE) != 0U) {
    send_gesture_command(
        GESTURE_COMMAND_ACTIVATE,
        BUZZER_SOUND_ACTIVATION,
        "ACTIVATE");
  }

  /*
   * Gesture verso sinistra:
   * richiesta volontaria di soccorso.
   */
  if ((flags & EVT_GESTURE_LEFT) != 0U) {
    send_gesture_command(
        GESTURE_COMMAND_SOS,
        BUZZER_SOUND_LEFT,
        "SOS");
  }

  /*
   * Gesture verso destra:
   * segnalazione che la zona è sicura.
   */
  if ((flags & EVT_GESTURE_RIGHT) != 0U) {
    send_gesture_command(
        GESTURE_COMMAND_SAFE_ZONE,
        BUZZER_SOUND_RIGHT,
        "SAFE_ZONE");
  }
}

/*===========================================================================*/
/* Thread applicativo.                                                       */
/*===========================================================================*/

static THD_WORKING_AREA(
    wa_gesture_application,
    1536);

static THD_FUNCTION(
    gesture_application_thread,
    argument) {

  event_listener_t gesture_listener;

  (void)argument;

  chRegSetThreadName("gesture-app");

  /*
   * Inizializza il collegamento con la board principale.
   *
   * Se la periferica non è disponibile, il thread riprova senza bloccare
   * definitivamente il sistema operativo.
   */
  while (!gesture_link_tx_start()) {
    chprintf(
        debug_stream,
        "GESTURE LINK: inizializzazione fallita, nuovo tentativo\r\n");

    chThdSleepMilliseconds(
        GESTURE_LINK_RETRY_DELAY_MS);
  }

  chprintf(
      debug_stream,
      "GESTURE LINK: collegamento TX inizializzato\r\n");

  /*
   * La registrazione deve essere eseguita dal thread che successivamente
   * attenderà gli eventi.
   *
   * Viene eseguita prima di gesture_init() per evitare di perdere una
   * gesture eventualmente riconosciuta subito dopo l'avvio del sensore.
   */
  chEvtRegisterMask(
      &gesture_events,
      &gesture_listener,
      EVENT_MASK(0));

  /*
   * Inizializza il VL53L7CX e avvia il thread interno di acquisizione e
   * riconoscimento.
   */
  gesture_init(debug_stream);

  chprintf(
      debug_stream,
      "GESTURE: sensore inizializzato, sistema pronto\r\n");

  while (true) {
    eventflags_t flags;

    /*
     * Attende almeno un evento proveniente dal modulo gesture.
     */
    (void)chEvtWaitAny(EVENT_MASK(0));

    /*
     * Recupera e cancella tutte le flag gesture pendenti.
     */
    flags = chEvtGetAndClearFlags(
        &gesture_listener);

    process_gesture_events(flags);
  }
}

/*===========================================================================*/
/* API pubblica.                                                             */
/*===========================================================================*/

void gesture_application_start(void) {

  /*
   * Protezione contro una seconda chiamata accidentale.
   *
   * Questa funzione viene normalmente chiamata dal main prima che inizi
   * la normale esecuzione concorrente dell'applicazione.
   */
  if (application_started) {
    return;
  }

  application_started = true;

  debug_serial_start();

  chprintf(
      debug_stream,
      "\r\nNISC2026 Gesture Board\r\n");

  /*
   * buzzer_init() configura TIM3_CH1 su PB4, avvia il proprio thread e
   * riproduce automaticamente il suono di accensione.
   */
  buzzer_init();

  /*
   * Il thread applicativo inizializzerà prima il collegamento TX e poi il
   * sensore, registrandosi agli eventi gesture.
   */
  chThdCreateStatic(
      wa_gesture_application,
      sizeof(wa_gesture_application),
      NORMALPRIO + 1,
      gesture_application_thread,
      NULL);
}
