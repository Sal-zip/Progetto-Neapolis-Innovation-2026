#include "thread_mq2.h"

#include <stddef.h>
#include <string.h>

/* Soglie originali del progetto MQ-2. */
#define MQ2_EMERGENCY_HIGH_THRESHOLD  700U
#define MQ2_EMERGENCY_LOW_THRESHOLD   150U

/* Buzzer passivo collegato a PB4/TIM3_CH1. */
#define MQ2_BUZZER_PORT           GPIOB
#define MQ2_BUZZER_PIN              4U
#define MQ2_BUZZER_PWM_CHANNEL      0U

/*
 * Configurazione PWM originale:
 *
 * frequenza timer = 1 MHz
 * periodo         = 400 tick
 * frequenza PWM   = 1.000.000 / 400 = 2.500 Hz
 * duty cycle      = 200 / 400 = 50%
 */
#define MQ2_BUZZER_TIMER_FREQUENCY  1000000U
#define MQ2_BUZZER_PWM_PERIOD       400U
#define MQ2_BUZZER_PWM_DUTY         200U

/* Intervallo di campionamento originale. */
#define MQ2_SAMPLE_INTERVAL_SECONDS 1U

static const PWMConfig mq2_buzzer_pwm_config = {
  .frequency = MQ2_BUZZER_TIMER_FREQUENCY,
  .period = MQ2_BUZZER_PWM_PERIOD,
  .callback = NULL,

  .channels = {
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL}
  },

  .cr2 = 0U,
  .dier = 0U
};

/* Working area originale del thread MQ-2. */
static THD_WORKING_AREA(wa_mq2_thread, 256);

/* Ultimo risultato reso disponibile all'Application. */
static mq2_data_t latest_mq2_data;
static mutex_t latest_mq2_mutex;
static systime_t latest_mq2_update;

static bool latest_mq2_available;
static bool mq2_service_started;

/**
 * Classifica il valore usando le soglie originali.
 */
static mq2_status_t mq2_classify(adcsample_t raw_adc) {
  if (raw_adc > MQ2_EMERGENCY_HIGH_THRESHOLD) {
    return MQ2_STATUS_HIGH;
  }

  if (raw_adc < MQ2_EMERGENCY_LOW_THRESHOLD) {
    return MQ2_STATUS_LOW;
  }

  return MQ2_STATUS_NORMAL;
}

/**
 * Restituisce true per entrambe le condizioni di emergenza originali.
 */
static bool mq2_is_emergency(adcsample_t raw_adc) {
  return
      (raw_adc > MQ2_EMERGENCY_HIGH_THRESHOLD) ||
      (raw_adc < MQ2_EMERGENCY_LOW_THRESHOLD);
}

/**
 * Aggiorna in mutua esclusione il risultato visibile all'Application.
 */
static void mq2_store_latest(
    adcsample_t raw_adc,
    mq2_status_t status,
    bool emergency,
    bool buzzer_on) {
  chMtxLock(&latest_mq2_mutex);

  latest_mq2_data.raw_adc = raw_adc;
  latest_mq2_data.status = status;
  latest_mq2_data.emergency = emergency;
  latest_mq2_data.buzzer_on = buzzer_on;
  latest_mq2_data.valid = true;

  latest_mq2_update = chVTGetSystemTimeX();
  latest_mq2_available = true;

  chMtxUnlock(&latest_mq2_mutex);
}

/**
 * Thread periodico MQ-2 e buzzer.
 */
static THD_FUNCTION(mq2_thread, argument) {
  (void)argument;

  chRegSetThreadName("mq2");

  /*
   * Avvia TIM3. Come nell'implementazione originale,
   * il buzzer inizialmente è disabilitato.
   */
  pwmStart(&PWMD3, &mq2_buzzer_pwm_config);
  pwmDisableChannel(&PWMD3, MQ2_BUZZER_PWM_CHANNEL);

  bool buzzer_on = false;

  while (true) {
    mq2_sample_t sample = {0U};

    if (mq2_read_sample(&sample)) {
      const bool emergency =
          mq2_is_emergency(sample.raw_adc);

      /*
       * Attiva il buzzer soltanto quando si entra
       * nello stato di emergenza.
       */
      if (emergency && !buzzer_on) {
        pwmEnableChannel(
            &PWMD3,
            MQ2_BUZZER_PWM_CHANNEL,
            MQ2_BUZZER_PWM_DUTY);

        buzzer_on = true;
      }
      /*
       * Disattiva il buzzer soltanto quando si ritorna
       * nell'intervallo normale.
       */
      else if (!emergency && buzzer_on) {
        pwmDisableChannel(
            &PWMD3,
            MQ2_BUZZER_PWM_CHANNEL);

        buzzer_on = false;
      }

      mq2_store_latest(
          sample.raw_adc,
          mq2_classify(sample.raw_adc),
          emergency,
          buzzer_on);
    }

    chThdSleepSeconds(MQ2_SAMPLE_INTERVAL_SECONDS);
  }
}

void mq2_start(void) {
  if (mq2_service_started) {
    return;
  }

  memset(&latest_mq2_data, 0, sizeof(latest_mq2_data));

  latest_mq2_update = 0U;
  latest_mq2_available = false;

  chMtxObjectInit(&latest_mq2_mutex);

  /*
   * Inizializza PA0 e ADC1.
   * La correzione PA4 -> PA0 è contenuta in mq2_init().
   */
  (void)mq2_init();

  /*
   * Configura PB4 come uscita alternativa TIM3_CH1.
   */
  palSetPadMode(
      MQ2_BUZZER_PORT,
      MQ2_BUZZER_PIN,
      PAL_MODE_ALTERNATE(2));

  mq2_service_started = true;

  (void)chThdCreateStatic(
      wa_mq2_thread,
      sizeof(wa_mq2_thread),
      NORMALPRIO,
      mq2_thread,
      NULL);
}

bool mq2_get_latest(
    mq2_data_t *out,
    sysinterval_t max_age) {
  if ((out == NULL) || !mq2_service_started) {
    return false;
  }

  bool available = false;

  chMtxLock(&latest_mq2_mutex);

  if (latest_mq2_available) {
    const sysinterval_t age =
        chVTTimeElapsedSinceX(latest_mq2_update);

    if ((max_age == TIME_INFINITE) || (age <= max_age)) {
      *out = latest_mq2_data;
      available = true;
    }
  }

  chMtxUnlock(&latest_mq2_mutex);

  return available;
}
