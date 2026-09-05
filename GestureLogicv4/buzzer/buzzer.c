/*
    NISC2026 - GestureLogicv3
    Modulo: buzzer di feedback per l'operatore (buzzer passivo su PA6).

    Hardware: buzzer passivo su PA6, canale TIM3_CH1 (PWM, alternate 2).
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

    Suoni (per distinguerli a orecchio basta la tonalita', tutti brevi):
      - POWER_ON:    tre note crescenti (~0.5 s), il classico rumore di
                     accensione; parte da solo all'avvio del modulo;
      - ACTIVATION:  un bip breve di tonalita' media (sistema ATTIVATO
                     dopo il doppio gesto);
      - LEFT:        un bip breve grave (azione sinistra);
      - RIGHT:       un bip breve acuto (azione destra).

    Le melodie si ritoccano qui, senza toccare chi chiama buzzer_play().
*/

#include "buzzer.h"

/* PROVA DIAGNOSTICA TEMPORANEA: a 1 suona una nota continua a rotazione su
   PB6 (TIM4_CH1), PA6 (TIM3_CH1) e PB4 (TIM3_CH1) per individuare il pin
   dove e' fisicamente cablato il buzzer. A diagnosi conclusa impostare a 0. */
#define BUZZER_PIN_PROBE    1U

#if BUZZER_PIN_PROBE
#include "chprintf.h"
#endif

/* ========================================================================= */
/* Configurazione hardware (MODIFICARE per adattarla al cablaggio reale)     */
/* ========================================================================= */

/* Pin del buzzer (PWM TIM3_CH1 su PA6: non usato dagli altri GPIO). */
#define BUZZER_LINE         PAL_LINE(GPIOB, 6U)

/* Driver PWM del timer TIM3. */
#define BUZZER_PWM_DRV      &PWMD4

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

/* Sistema ATTIVATO (doppio gesto): un bip breve di tonalita' media. */
static const buzzer_note_t sound_activation[] = {
  { 2637U, 120U },
  {    0U,   0U }
};

/* Azione sinistra: un bip breve grave, "tu" basso. */
static const buzzer_note_t sound_left[] = {
  { 1047U, 120U },
  {    0U,   0U }
};

/* Azione destra: un bip breve acuto, "tu" alto. */
static const buzzer_note_t sound_right[] = {
  { 3520U, 120U },
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

#if BUZZER_PIN_PROBE
/* --- PROVA DIAGNOSTICA PIN (temporanea) ------------------------------------
 * All'avvio suona 2 s di nota continua per ogni pin candidato, in ciclo:
 *   1. PB6 -> TIM4_CH1 (config attuale del modulo, PWMD4, AF2);
 *   2. PA6 -> TIM3_CH1 (config indicata dai commenti, PWMD3, AF2);
 *   3. PB4 -> TIM3_CH1 (config v3/helmet verificata su hardware, PWMD3, AF2).
 * Il tono si sente SOLO nella fase del pin dove arriva il filo del buzzer.
 * La seriale (SD2) stampa ogni fase. Nessun suono in nessuna fase = il filo
 * non e' su nessuno di questi pin, oppure il firmware flashato non e' questo.
 * -------------------------------------------------------------------------- */

#define PROBE_HZ   2200U
#define PROBE_MS   2000U
#define PROBE_GAP  400U

static void probe_tone_on(PWMDriver *pwmp, ioline_t line) {

  palSetLineMode(line, PAL_MODE_ALTERNATE(2));
  pwmStart(pwmp, &s_pwm_cfg);
  pwmChangePeriod(pwmp, BUZZER_TIM_HZ / PROBE_HZ);
  pwmEnableChannel(pwmp, 0U, (BUZZER_TIM_HZ / PROBE_HZ) / 2U);
}

static void probe_tone_off(PWMDriver *pwmp, ioline_t line) {

  pwmDisableChannel(pwmp, 0U);
  pwmStop(pwmp);
  palSetLineMode(line, PAL_MODE_INPUT);
}

static void buzzer_probe(void) {

  BaseSequentialStream *chp = (BaseSequentialStream *)&SD2;

  while (true) {
    chprintf(chp, "[PROBE] 1/3 nota su PB6  (TIM4_CH1, PWMD4)...\r\n");
    probe_tone_on(&PWMD4, PAL_LINE(GPIOB, 6U));
    chThdSleepMilliseconds(PROBE_MS);
    probe_tone_off(&PWMD4, PAL_LINE(GPIOB, 6U));
    chThdSleepMilliseconds(PROBE_GAP);

    chprintf(chp, "[PROBE] 2/3 nota su PA6  (TIM3_CH1, PWMD3)...\r\n");
    probe_tone_on(&PWMD3, PAL_LINE(GPIOA, 6U));
    chThdSleepMilliseconds(PROBE_MS);
    probe_tone_off(&PWMD3, PAL_LINE(GPIOA, 6U));
    chThdSleepMilliseconds(PROBE_GAP);

    chprintf(chp, "[PROBE] 3/3 nota su PB4  (TIM3_CH1, PWMD3)...\r\n");
    probe_tone_on(&PWMD3, PAL_LINE(GPIOB, 4U));
    chThdSleepMilliseconds(PROBE_MS);
    probe_tone_off(&PWMD3, PAL_LINE(GPIOB, 4U));
    chThdSleepMilliseconds(PROBE_GAP);
  }
}
#endif /* BUZZER_PIN_PROBE */

static THD_FUNCTION(buzzer_thread, arg) {

  (void)arg;
  chRegSetThreadName("buzzer");

  event_listener_t el;
  chEvtRegisterMask(&s_buzzer_events, &el, EVENT_MASK(0));

#if BUZZER_PIN_PROBE
  /* Prova diagnostica: ciclo infinito di note sui pin candidati. */
  buzzer_probe();
#else
  /* Suono di accensione: parte da solo, una volta, all'avvio. */
  melody_play(sound_power_on);
#endif

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

    chThdSleep(20);
  }
}

/* ========================================================================= */
/* API pubblica                                                              */
/* ========================================================================= */

void buzzer_init(void) {

  /* Linea del buzzer in PWM (TIM4_CH1, alternate 2). */
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
