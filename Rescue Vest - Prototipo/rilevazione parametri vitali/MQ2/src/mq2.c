#include "mq2.h"

#include <stddef.h>

/*
 * Collegamento analogico corretto per STM32G474RE:
 *
 * PA0 -> ADC1_IN1
 */
#define MQ2_ADC_PORT       GPIOA
#define MQ2_ADC_PIN        1U
#define MQ2_ADC_CHANNEL    ADC_CHANNEL_IN2

/**
 * @brief Configurazione della conversione ADC del sensore MQ-2.
 *
 * Mantiene gli stessi parametri presenti nel progetto originale:
 * - una sola conversione;
 * - un solo canale;
 * - ADC1_IN1;
 * - nessuna callback;
 * - sample time originale.
 */
static const ADCConversionGroup mq2_adc_group = {
  .circular     = false,
  .num_channels = 1U,
  .end_cb       = NULL,
  .error_cb     = NULL,

  .cfgr         = ADC_CFGR_CONT,

  .tr1          = 0U,
  .tr2          = 0U,
  .tr3          = 0U,

  .awd2cr       = 0U,
  .awd3cr       = 0U,

  .smpr[0]      = 0U,
  .smpr[1]      = 0U,

  .sqr[0]       = ADC_SQR1_SQ1_N(MQ2_ADC_CHANNEL),
  .sqr[1]       = 0U,
  .sqr[2]       = 0U,
  .sqr[3]       = 0U
};

bool mq2_init(void) {
  /*
   * Nel progetto originale veniva configurato erroneamente PA4.
   * ADC1_IN1 sullo STM32G474RE corrisponde invece a PA0.
   */
  palSetPadMode(
      MQ2_ADC_PORT,
      MQ2_ADC_PIN,
      PAL_MODE_INPUT_ANALOG);

  /* Avvia il driver ADC1. */
  adcStart(&ADCD1, NULL);

  return true;
}

bool mq2_read_sample(mq2_sample_t *sample) {
  if (sample == NULL) {
    return false;
  }

  adcsample_t adc_value = 0U;

  /*
   * Protegge ADC1 qualora venga condiviso in futuro
   * con altri sensori analogici.
   */
  adcAcquireBus(&ADCD1);

  const msg_t status = adcConvert(
      &ADCD1,
      &mq2_adc_group,
      &adc_value,
      1U);

  adcReleaseBus(&ADCD1);

  if (status != MSG_OK) {
    sample->raw_adc = 0U;
    return false;
  }

  sample->raw_adc = adc_value;

  return true;
}
