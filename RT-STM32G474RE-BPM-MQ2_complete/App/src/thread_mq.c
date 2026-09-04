#include "thread_mq.h"
#include "chprintf.h"
#include <stdbool.h>

/* ===========================================================================
 * ADC Configuration (MQ-2 on ADC1_IN1 = PA0)
 * =========================================================================*/
static const ADCConversionGroup adcgrpcfg = {
  .circular     = false,
  .num_channels = 1,
  .end_cb       = NULL,
  .error_cb     = NULL,
  .cfgr         = ADC_CFGR_CONT,
  .tr1          = 0,
  .tr2          = 0,
  .tr3          = 0,
  .awd2cr       = 0,
  .awd3cr       = 0,
  .smpr[0]      = 0,
  .smpr[1]      = 0,
  .sqr[0]       = ADC_SQR1_SQ1_N(MQ2_ADC_CHANNEL),
  .sqr[1]       = 0,
  .sqr[2]       = 0,
  .sqr[3]       = 0
};

/* ===========================================================================
 * PWM Configuration (Buzzer on PB4 = TIM3_CH1, 2.5kHz)
 * =========================================================================*/
#define BUZZER_TIMER_HZ  1000000
#define BUZZER_PERIOD    400
#define BUZZER_DUTY      200

static const PWMConfig pwmcfg = {
  .frequency = BUZZER_TIMER_HZ,
  .period    = BUZZER_PERIOD,
  .callback  = NULL,
  .channels  = {
    {PWM_OUTPUT_ACTIVE_HIGH, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_DISABLED, NULL}
  },
  .cr2  = 0,
  .dier = 0
};

/* ===========================================================================
 * Thread MQ-2 + Buzzer
 * =========================================================================*/
THD_WORKING_AREA(wa_thread_mq2, 256);

THD_FUNCTION(thread_mq, arg) {
  (void)arg;
  chRegSetThreadName("mq2");

  /* Start PWM (buzzer disabled initially) */
  pwmStart(&PWMD3, &pwmcfg);
  pwmDisableChannel(&PWMD3, 0);

  bool buzzer_on = false;

  while (true) {
    adcsample_t mq2_adc_val = 0;
    adcAcquireBus(&ADCD1);
    adcConvert(&ADCD1, &adcgrpcfg, &mq2_adc_val, 1);
    adcReleaseBus(&ADCD1);

    bool emergenza = (mq2_adc_val > MQ2_EMERGENZA_ALTA) ||
                     (mq2_adc_val < MQ2_EMERGENZA_BASSA);

    chprintf(chp, "ADC: %lu\r\n", (uint32_t)mq2_adc_val);
    if (emergenza && !buzzer_on) {
      pwmEnableChannel(&PWMD3, 0, BUZZER_DUTY);
      buzzer_on = true;
      chMtxLock(&serial_mtx);
      chprintf(chp, "EMERGENZA, ARIA IRRESPIRABILE | ADC %lu\r\n", (uint32_t)mq2_adc_val);
      chMtxUnlock(&serial_mtx);
    } else if (!emergenza && buzzer_on) {
      pwmDisableChannel(&PWMD3, 0);
      buzzer_on = false;
    }

    chThdSleepSeconds(1);
  }
}
