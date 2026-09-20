/*
    NISC2026 - GestureLogicv3
    Modulo: buzzer di feedback per l'operatore (buzzer passivo su PB4).

    Hardware: buzzer passivo su PB4, canale TIM3_CH1 (PWM, alternate 2).
    Il buzzer passivo produce un tono solo se pilotato con un'onda quadra
    alla frequenza desiderata: qui il PWM genera l'onda (duty 50%) e la
    frequenza viene cambiata nota per nota.

    I pattern sono eseguiti da un thread interno dedicato (il PWM da solo
    non sa fare pause/melodie): chi vuole un suono fa una richiesta con
    chEvtBroadcastFlags() su `s_buzzer_events`, il thread la prende e suona.

    NOTA SUL DRIVER PWM: in questa HAL il driver va avviato UNA SOLA volta
    (pwmStart() in buzzer_init()); per cambiare la frequenza si usa
    pwmChangePeriod() a driver gia' attivo e per il silenzio si disabilita
    il canale (pwmDisableChannel(), CCR=0). Niente pwmStop()/pwmStart() a
    ogni nota: ogni start ri-abilita i canali configurati e riazzera il
    timer, quindi se la sequenza si interrompe il tono resta acceso per
    sempre (suono continuo che non smette).

    Suoni (per distinguerli a orecchio basta nota E ritmo):
      - POWER_ON:    tre note crescenti (~0.5 s), il classico rumore di
                     accensione; parte da solo all'avvio del modulo;
      - ACTIVATION:  tre beep corti uguali (sistema ATTIVATO dopo il
                     doppio gesto);
      - LEFT:        una nota grave lunga (azione sinistra);
      - RIGHT:       due note acute ravvicinate (azione destra).

    Le melodie si ritoccano qui, senza toccare chi chiama buzzer_play().
*/

#include "buzzer.h"

/* ========================================================================= */
/* Configurazione hardware (MODIFICARE per adattarla al cablaggio reale)     */
/* ========================================================================= */

/* Pin del buzzer (PWM TIM3_CH1). */
#define BUZZER_LINE         PAL_LINE(GPIOB, 4U)

/* Driver PWM del timer TIM3. */
#define BUZZER_PWM_DRV      &PWMD3

/* Tick del timer PWM: 1 MHz (come helmet_buzzer.c). */
#define BUZZER_TIM_HZ       1000000U

/* Pausa fissa tra un suono e il successivo (ms). */
#define BUZZER_GAP_MS       180U

/* ========================================================================= */
/* Pattern sonori (freq 0 = pausa; la melodia termina con durata 0)          */
/* ========================================================================= */

typedef struct {
  uint16_t freq_hz;          /* Frequenza della nota (0 = pausa). */
  uint16_t dur_ms;           /* Durata della nota / pausa. */
} buzzer_note_t;

/* Accensione: tre note crescenti (Do5-Mi5-La5), classico bip di boot. */
static const buzzer_note_t sound_power_on[] = {
  { 2093U,  90U },
  {    0U,  60U },
  { 2794U,  90U },
  {    0U,  60U },
  { 3520U, 170U },
  {    0U,   0U }            /* Fine melodia. */
};

/* Azione sinistra: una sola nota grave, "tuu". */
static const buzzer_note_t sound_left[] = {
  { 2349U, 160U },
  {    0U,   0U }
};

/* Sistema ATTIVATO (doppio gesto): tre beep corti uguali, "bip-bip-bip". */
static const buzzer_note_t sound_activation[] = {
  { 2637U,  80U },
  {    0U,  50U },
  { 2637U,  80U },
  {    0U,  50U },
  { 2637U,  80U },
  {    0U,   0U }
};

/* Azione destra: due note acute ravvicinate, "tu-tu". */
static const buzzer_note_t sound_right[] = {
  { 3520U,  90U },
  {    0U,  60U },
  { 3520U,  90U },
  {    0U,   0U }
};

/* ========================================================================= */
/* Stato del modulo                                                          */
/* ========================================================================= */

/* Configurazione PWM (driver avviato una volta sola, poi solo cambio nota). */
static PWMConfig s_pwm_cfg = {
  BUZZER_TIM_HZ,
  BUZZER_TIM_HZ / 2500U,     /* Periodo iniziale ~2.5 kHz. */
  NULL,
  {
    { PWM_OUTPUT_ACTIVE_HIGH, NULL },
    { PWM_OUTPUT_DISABLED,    NULL },
    { PWM_OUTPUT_DISABLED,    NULL },
    { PWM_OUTPUT_DISABLED,    NULL }
  },
  0,
  0,
  0
};

/* Event source interno: le flag sono le richieste di suono in attesa. */
#define BUZZER_FLAG_POWER_ON    ((eventflags_t)(1U << 0))
#define BUZZER_FLAG_ACTIVATION  ((eventflags_t)(1U << 1))
#define BUZZER_FLAG_LEFT        ((eventflags_t)(1U << 2))
#define BUZZER_FLAG_RIGHT       ((eventflags_t)(1U << 3))

static EVENTSOURCE_DECL(s_buzzer_events);

static THD_WORKING_AREA(wa_buzzer, 512);

/* ========================================================================= */
/* Generazione dei toni (PWM)                                                */
/* ========================================================================= */

/* Accende la nota: imposta il periodo (frequenza) e abilita il canale
 * a duty 50%. Il driver resta avviato, si cambia solo il periodo. */
static void tone_on(uint16_t freq_hz) {

  pwmcnt_t period = BUZZER_TIM_HZ / freq_hz;
  if (period < 2U) {
    period = 2U;
  }

  pwmChangePeriod(BUZZER_PWM_DRV, period);
  pwmEnableChannel(BUZZER_PWM_DRV, 0U, period / 2U);
}

/* Silenzio: canale disabilitato (CCR=0, uscita bassa). */
static void tone_off(void) {

  pwmDisableChannel(BUZZER_PWM_DRV, 0U);
}

/* Esegue una melodia nota per nota (bloccante: solo nel thread del buzzer). */
static void melody_play(const buzzer_note_t *notes) {

  while (notes->dur_ms != 0U) {
    if (notes->freq_hz != 0U) {
      tone_on(notes->freq_hz);
    }
    else {
      tone_off();
    }
    chThdSleepMilliseconds(notes->dur_ms);
    notes++;
  }

  /* Silenzio tra un suono e il successivo. */
  tone_off();
  chThdSleepMilliseconds(BUZZER_GAP_MS);
}

/* ========================================================================= */
/* Thread interno: esegue i suoni richiesti                                  */
/* ========================================================================= */

static THD_FUNCTION(buzzer_thread, arg) {

  (void)arg;
  chRegSetThreadName("buzzer");

  event_listener_t el;
  chEvtRegisterMask(&s_buzzer_events, &el, EVENT_MASK(0));

  /* Suono di accensione: parte da solo, una volta, all'avvio. */
  melody_play(sound_power_on);

  while (true) {
    eventmask_t  evts  = chEvtWaitAny(EVENT_MASK(0));
    eventflags_t flags = chEvtGetAndClearFlags(&el);

    (void)evts;

    /* Esegue in ordine i suoni rimasti pendenti (le flag si sommano). */
    if (flags & BUZZER_FLAG_POWER_ON) {
      melody_play(sound_power_on);
    }
    if (flags & BUZZER_FLAG_ACTIVATION) {
      melody_play(sound_activation);
    }
    if (flags & BUZZER_FLAG_LEFT) {
      melody_play(sound_left);
    }
    if (flags & BUZZER_FLAG_RIGHT) {
      melody_play(sound_right);
    }
  }
}

/* ========================================================================= */
/* API pubblica                                                              */
/* ========================================================================= */

void buzzer_init(void) {

  /* Linea del buzzer in PWM (TIM3_CH1, alternate 2). */
  palSetLineMode(BUZZER_LINE, PAL_MODE_ALTERNATE(2));

  /* Driver PWM avviato una volta sola (canale spento: CCR=0). */
  pwmStart(BUZZER_PWM_DRV, &s_pwm_cfg);

  /* Thread che esegue i pattern sonori (suona l'accensione all'avvio). */
  chThdCreateStatic(wa_buzzer, sizeof(wa_buzzer), NORMALPRIO,
                    buzzer_thread, NULL);
}

void buzzer_play(buzzer_sound_t sound) {

  eventflags_t flag;

  switch (sound) {
    case BUZZER_SOUND_POWER_ON:
      flag = BUZZER_FLAG_POWER_ON;
      break;
    case BUZZER_SOUND_ACTIVATION:
      flag = BUZZER_FLAG_ACTIVATION;
      break;
    case BUZZER_SOUND_LEFT:
      flag = BUZZER_FLAG_LEFT;
      break;
    case BUZZER_SOUND_RIGHT:
      flag = BUZZER_FLAG_RIGHT;
      break;
    default:
      flag = 0U;
      break;
  }

  if (flag != 0U) {
    chEvtBroadcastFlags(&s_buzzer_events, flag);
  }
}
