/*
    NISC2026 - GestureLogicv3
    Modulo: demo STAND-IN (vedi demo.h).

    SUBSCRIBER del modulo gesture: il consumer fittizio riceve le flag su
    `gesture_events` e fa la NOTIFY (stampe seriali + buzzer_play()).
    VERO PROPRIETARIO (in integrazione): chi realizza il modulo che deve
    reagire ai gesti; creera' il suo thread con chEvtRegisterMask() e
    chiamera' le sue funzioni reali al posto delle stampe/buzzer.
*/

#include "demo.h"
#include "chprintf.h"

#include "gesture_events.h"
#include "buzzer.h"

/* Flusso di debug della demo (puo' essere NULL). */
static BaseSequentialStream *s_dbg = NULL;

/* ========================================================================= */
/* LED verde di demo: heartbeat del programma (codice demo, non dei moduli   */
/* gesture/buzzer). Rimuovere o lasciare a discrezione del team.             */
/* ========================================================================= */

static THD_WORKING_AREA(waBlinker, 256);
static THD_FUNCTION(blinker_thread, arg) {

  (void)arg;
  chRegSetThreadName("blinker");

  while (true) {
    palClearPad(GPIOA, GPIOA_LED_GREEN);
    chThdSleepMilliseconds(500);
    palSetPad(GPIOA, GPIOA_LED_GREEN);
    chThdSleepMilliseconds(500);
  }
}

/* ========================================================================= */
/* Consumer fittizio dei gesti riconosciuti dal modulo gesture.              */
/* ========================================================================= */

static THD_WORKING_AREA(waConsumer, 256);
static THD_FUNCTION(consumer_thread, arg) {

  (void)arg;
  chRegSetThreadName("consumer");

  /* SUBSCRIBER: si registra sugli eventi pubblicati dal modulo gesture. */
  event_listener_t el;
  chEvtRegisterMask(&gesture_events, &el, EVENT_MASK(0));

  while (true) {
    eventmask_t  evts  = chEvtWaitAny(EVENT_MASK(0));
    eventflags_t flags = chEvtGetAndClearFlags(&el);

    (void)evts;

    if (flags & EVT_GESTURE_DOUBLE_WAVE) {
      /* --- INIZIO STAND-IN: NOTIFY - doppio movimento completato ---
         In integrazione qui il modulo proprietario avvia le funzionalita'
         del sistema (es. attiva la gestione degli operatori). Il buzzer
         conferma all'operatore che il sistema e' ATTIVATO (un bip breve
         di tonalita' media). */
      buzzer_play(BUZZER_SOUND_ACTIVATION);
      chprintf(s_dbg, "\r\n[!!!] SISTEMA ATTIVATO [!!!]\r\n\r\n");
      /* --- FINE STAND-IN (NOTIFY) --- */
    }
    if (flags & EVT_GESTURE_LEFT) {
      /* --- INIZIO STAND-IN: NOTIFY - azione laterale sinistra ---
         In integrazione qui si chiama la funzione reale del modulo che
         gestisce l'azione sinistra. Il buzzer conferma all'operatore che
         il gesto sinistro e' stato riconosciuto (un bip breve grave). */
      buzzer_play(BUZZER_SOUND_LEFT);
      chprintf(s_dbg, "\r\n<--- AZIONE SINISTRA RILEVATA! (Mano Aperta/Chiusa)\r\n");
      /* --- FINE STAND-IN (NOTIFY) --- */
    }
    if (flags & EVT_GESTURE_RIGHT) {
      /* --- INIZIO STAND-IN: NOTIFY - azione laterale destra ---
         Il buzzer conferma all'operatore che il gesto destro e'
         stato riconosciuto (un bip breve acuto). */
      buzzer_play(BUZZER_SOUND_RIGHT);
      chprintf(s_dbg, "\r\n---> AZIONE DESTRA RILEVATA! (Mano Aperta/Chiusa)\r\n");
      /* --- FINE STAND-IN (NOTIFY) --- */
    }
  }
}

/* ========================================================================= */
/* API pubblica                                                              */
/* ========================================================================= */

void demo_init(BaseSequentialStream *dbg) {

  s_dbg = dbg;

  /* GPIO demo non usati dai moduli gesture/buzzer (LED/attuatori degli
     altri membri del team): qui solo configurati a livello iniziale.
     PA6 escluso: e' il buzzer (PWM TIM3_CH1 del modulo buzzer/). */
  palSetPadMode(GPIOC, 0U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOA, 0U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOA, 5U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOA, 8U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOA, 9U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOB, 0U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOB, 10U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOB, 4U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOB, 5U, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPadMode(GPIOB, 3U, PAL_MODE_OUTPUT_PUSHPULL);

  palSetLine(PAL_LINE(GPIOC, 0U));
  palSetLine(PAL_LINE(GPIOA, 0U));
  palClearLine(PAL_LINE(GPIOA, 5U));
  palClearLine(PAL_LINE(GPIOA, 8U));
  palClearLine(PAL_LINE(GPIOA, 9U));
  palSetLine(PAL_LINE(GPIOB, 0U));
  palSetLine(PAL_LINE(GPIOB, 10U));
  palSetLine(PAL_LINE(GPIOB, 4U));
  palSetLine(PAL_LINE(GPIOB, 5U));
  palSetLine(PAL_LINE(GPIOB, 3U));
  palClearLine(PAL_LINE(GPIOB, 3U));

  /* Thread demo: LED heartbeat e consumer fittizio. */
  chThdCreateStatic(waBlinker, sizeof(waBlinker), NORMALPRIO + 1,
                    blinker_thread, NULL);
  chThdCreateStatic(waConsumer, sizeof(waConsumer), NORMALPRIO,
                    consumer_thread, NULL);
}
