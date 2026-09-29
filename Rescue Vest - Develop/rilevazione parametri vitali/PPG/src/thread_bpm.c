#include "thread_bpm.h"
#include <stdbool.h>

/* Configurazione Pin ADC: PA0 (A0) */
#define PPG_ADC_PORT    GPIOA
#define PPG_ADC_PIN     0U

/* Buffer ADC e variabili globali */
static adcsample_t adc_buffer[1];
static uint16_t current_bpm    = 0;
static uint16_t debug_last_raw = 0;

/* Configurazione ADC: PA0 → ADC1_IN1 */
static const ADCConversionGroup adc_grp_config = {
  .circular     = false,
  .num_channels = 1U,
  .end_cb       = NULL,
  .error_cb     = NULL,
  .cfgr         = 0U,
  .tr1          = 0U,
  .tr2          = 0U,
  .tr3          = 0U,
  .awd2cr       = 0U,
  .awd3cr       = 0U,
  .smpr         = {
      ADC_SMPR1_SMP_AN1(ADC_SMPR_SMP_247P5),
      0U
    },
    .sqr          = {
      ADC_SQR1_NUM_CH(1U) | ADC_SQR1_SQ1_N(ADC_CHANNEL_IN1),
      0U,
      0U,
      0U
    }
};

static THD_WORKING_AREA(waBPMThread, 256);

static THD_FUNCTION(BPMThread, arg) {
  (void)arg;
  chRegSetThreadName("bpm_reader");

  ppg_sample_t sample;

  uint32_t timer_ticks     = 0;
  uint32_t last_beat_ticks = 0;
  uint16_t prev_raw        = 0; /* campione precedente per il 2-sample smooth */
  uint16_t prev_s          = 0; /* smoothed precedente per il rising edge */

  /* Buffer media mobile: ultimi 4 battiti */
  #define BPM_HISTORY_SIZE 4
  uint16_t bpm_history[BPM_HISTORY_SIZE] = {0};
  uint8_t  bpm_idx = 0;

  /*
   * HIGH = 3000:
   *   - segnale a riposo (senza dito): ~2000    → sempre sotto HIGH ✓
   *   - segnale tra un battito e l'altro: ~2833 → sotto HIGH ✓
   *   - segnale durante il picco del battito: ~3200-3900 → sopra HIGH ✓
   *
   * Il segnale scende naturalmente sotto HIGH tra i battiti,
   * quindi NON serve un LOW threshold: basta il periodo refrattario.
   *
   * REFRACTORY = 35 tick × 10ms = 350ms → massimo ~170 BPM
   * TIMEOUT    = 500 tick × 10ms = 5s   → azzeramento se dito rimosso
   */
  const uint16_t HIGH       = 3000U;
  const uint32_t REFRACTORY = 35U;
  const uint32_t TIMEOUT    = 500U;

  while (true) {
    if (ppg_read_sample(&sample)) {
      uint16_t raw = sample.raw;
      debug_last_raw = raw;

      /* Smoothing a 2 campioni: elimina spike singoli senza appiattire l'onda.
       * Con finestra di soli 20ms, il valley smoothato resta vicino al raw (~2833)
       * e il peak smoothato resta alto (~3300+), garantendo che HIGH=3000
       * venga attraversato in entrambe le direzioni ogni battito. */
      uint16_t s = (raw + prev_raw) / 2U;
      prev_raw = raw;

      /* Timeout: 5 secondi senza battito → dito rimosso */
      if ((timer_ticks - last_beat_ticks) > TIMEOUT) {
        current_bpm = 0;
        for (int i = 0; i < BPM_HISTORY_SIZE; i++) bpm_history[i] = 0;
        bpm_idx = 0;
      }

      /* RISING EDGE: smoothed supera HIGH venendo dal basso.
       * Ogni battito cardiaco genera esattamente un rising edge su HIGH=3000. */
      if (s > HIGH && prev_s <= HIGH) {
        uint32_t dt = timer_ticks - last_beat_ticks;

        if (dt >= REFRACTORY) {
          last_beat_ticks = timer_ticks;

          /* Range fisiologico: da 40 BPM (150 tick) a 170 BPM (35 tick) */
          if (dt <= 150U) {
            uint16_t bpm = (uint16_t)(60000UL / (dt * 10UL));

            bpm_history[bpm_idx] = bpm;
            bpm_idx = (bpm_idx + 1U) % BPM_HISTORY_SIZE;

            uint32_t sum = 0;
            uint8_t  cnt = 0;
            for (int i = 0; i < BPM_HISTORY_SIZE; i++) {
              if (bpm_history[i] > 0) { sum += bpm_history[i]; cnt++; }
            }
            if (cnt > 0) current_bpm = (uint16_t)(sum / cnt);
          }
        }
      }

      prev_s = s;
      timer_ticks++;
    }

    chThdSleepMilliseconds(10);
  }
}

bool ppg_init(void) {
  palSetPadMode(PPG_ADC_PORT, PPG_ADC_PIN, PAL_MODE_INPUT_ANALOG);
  adcStart(&ADCD1, NULL);
  return true;
}

bool ppg_read_sample(ppg_sample_t *sample) {
  if (sample == NULL) return false;
  msg_t status = adcConvert(&ADCD1, &adc_grp_config, adc_buffer, 1);
  if (status != MSG_OK) { sample->raw = 0U; return false; }
  sample->raw = (uint16_t)adc_buffer[0];
  return true;
}

void bpm_thread_start(void) {
  chThdCreateStatic(waBPMThread, sizeof(waBPMThread), NORMALPRIO + 1, BPMThread, NULL);
}

uint16_t bpm_get_value(void) { return current_bpm; }
uint16_t bpm_get_raw(void)   { return debug_last_raw; }
