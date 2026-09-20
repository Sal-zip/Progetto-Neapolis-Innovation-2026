#include "thread_ppg.h"

#include <stddef.h>
#include <string.h>

/* Numero di BPM utilizzati per la media mobile. */
#define PPG_BPM_AVERAGE_WINDOW       8U

/* Intervallo minimo accettato tra due picchi consecutivi. */
#define PPG_MIN_BEAT_INTERVAL_MS     300U

/* Tempo oltre il quale il battito viene considerato assente. */
#define PPG_NO_BEAT_TIMEOUT_MS       1000U

/* Soglie mantenute dal progetto originale. */
#define PPG_LOW_BPM_THRESHOLD        40U
#define PPG_HIGH_BPM_THRESHOLD       120U

/* Parametri originali dei filtri. */
#define PPG_DC_PREVIOUS_WEIGHT       0.95f
#define PPG_DC_SAMPLE_WEIGHT         0.05f
#define PPG_LP_PREVIOUS_WEIGHT       0.80f
#define PPG_LP_SAMPLE_WEIGHT         0.20f
#define PPG_PEAK_THRESHOLD           20.0f

/*
 * Condizioni interne utilizzate per verificare due rilevazioni
 * consecutive dello stesso tipo.
 */
typedef enum {
  PPG_CONDITION_NORMAL = 0,
  PPG_CONDITION_LOW,
  PPG_CONDITION_HIGH
} ppg_condition_t;

/* Working area originale del thread BPM. */
static THD_WORKING_AREA(wa_ppg_thread, 512);

/* Ultimo risultato reso disponibile all'Application. */
static ppg_data_t latest_ppg_data;
static mutex_t latest_ppg_mutex;
static systime_t latest_ppg_update;

static bool latest_ppg_available;
static bool ppg_service_started;

/**
 * Inserisce una nuova condizione nella cronologia di due elementi.
 */
static void ppg_update_condition_history(
    ppg_condition_t history[2],
    ppg_condition_t condition) {
  history[0] = history[1];
  history[1] = condition;
}

/**
 * Verifica se sono presenti due condizioni consecutive di emergenza.
 */
static bool ppg_is_emergency(
    const ppg_condition_t history[2]) {
  const bool low_emergency =
      (history[0] == PPG_CONDITION_LOW) &&
      (history[1] == PPG_CONDITION_LOW);

  const bool high_emergency =
      (history[0] == PPG_CONDITION_HIGH) &&
      (history[1] == PPG_CONDITION_HIGH);

  return low_emergency || high_emergency;
}

/**
 * Aggiorna in mutua esclusione il risultato visibile all'Application.
 */
static void ppg_store_latest(
    const ppg_sample_t *sample,
    uint32_t heart_rate_bpm,
    ppg_status_t status,
    bool emergency,
    bool valid) {
  chMtxLock(&latest_ppg_mutex);

  latest_ppg_data.heart_rate_bpm = heart_rate_bpm;
  latest_ppg_data.red_raw = sample->red;
  latest_ppg_data.infrared_raw = sample->infrared;
  latest_ppg_data.status = status;
  latest_ppg_data.emergency = emergency;
  latest_ppg_data.valid = valid;

  latest_ppg_update = chVTGetSystemTimeX();
  latest_ppg_available = true;

  chMtxUnlock(&latest_ppg_mutex);
}

/**
 * Thread di acquisizione ed elaborazione del segnale PPG.
 */
static THD_FUNCTION(ppg_thread, argument) {
  (void)argument;

  chRegSetThreadName("ppg");

  /*
   * Stato dei filtri, mantenuto invariato rispetto
   * all'algoritmo originale.
   */
  float infrared_dc_estimator = 0.0f;
  float infrared_ac_signal = 0.0f;
  float infrared_smoothed = 0.0f;

  bool peak_detected = false;
  bool first_sample = true;

  systime_t last_beat_time = chVTGetSystemTimeX();

  /*
   * Cronologia utilizzata per la media mobile dei BPM.
   */
  uint32_t bpm_history[PPG_BPM_AVERAGE_WINDOW] = {0U};
  uint8_t bpm_index = 0U;
  uint8_t bpm_count = 0U;
  uint32_t bpm_average = 0U;

  /*
   * Le due condizioni iniziali sono normali,
   * come nel progetto originale.
   */
  ppg_condition_t condition_history[2] = {
    PPG_CONDITION_NORMAL,
    PPG_CONDITION_NORMAL
  };

  while (true) {
    ppg_sample_t sample = {0U};

    if (!ppg_read_sample(&sample)) {
      /*
       * ppg_read_sample() esegue già un tentativo di recovery
       * e una seconda lettura. Se falliscono entrambe, questo
       * ciclo viene semplicemente scartato.
       */
      chThdSleepMilliseconds(15);
      continue;
    }

    const uint32_t infrared_value = sample.infrared;

    if (first_sample) {
      infrared_dc_estimator = (float)infrared_value;
      first_sample = false;
      last_beat_time = chVTGetSystemTimeX();
    }

    /*
     * Rimozione della componente continua.
     *
     * Equivale alla formula originale:
     * dc = dc * 0.95 + sample * 0.05
     */
    infrared_dc_estimator =
        (infrared_dc_estimator * PPG_DC_PREVIOUS_WEIGHT) +
        ((float)infrared_value * PPG_DC_SAMPLE_WEIGHT);

    infrared_ac_signal =
        (float)infrared_value - infrared_dc_estimator;

    /*
     * Filtro passa-basso:
     * smoothed = smoothed * 0.8 + ac * 0.2
     */
    infrared_smoothed =
        (infrared_smoothed * PPG_LP_PREVIOUS_WEIGHT) +
        (infrared_ac_signal * PPG_LP_SAMPLE_WEIGHT);

    /*
     * Rilevamento del fronte del picco.
     */
    if ((infrared_smoothed > PPG_PEAK_THRESHOLD) &&
        !peak_detected) {
      const systime_t current_time = chVTGetSystemTimeX();

      const uint32_t delta_ms =
          chTimeI2MS(current_time - last_beat_time);

      if (delta_ms > PPG_MIN_BEAT_INTERVAL_MS) {
        peak_detected = true;

        const uint32_t bpm = 60000U / delta_ms;

        /*
         * Inserisce il nuovo BPM nella finestra circolare.
         */
        bpm_history[bpm_index] = bpm;

        bpm_index =
            (uint8_t)((bpm_index + 1U) %
                      PPG_BPM_AVERAGE_WINDOW);

        if (bpm_count < PPG_BPM_AVERAGE_WINDOW) {
          ++bpm_count;
        }

        /*
         * Ricalcola la media degli ultimi battiti disponibili.
         */
        bpm_average = 0U;

        for (uint8_t index = 0U;
             index < bpm_count;
             ++index) {
          bpm_average += bpm_history[index];
        }

        bpm_average /= bpm_count;

        ppg_status_t status;
        ppg_condition_t condition;

        if (bpm_average < PPG_LOW_BPM_THRESHOLD) {
          status = PPG_STATUS_LOW;
          condition = PPG_CONDITION_LOW;
        } else if (bpm_average > PPG_HIGH_BPM_THRESHOLD) {
          status = PPG_STATUS_HIGH;
          condition = PPG_CONDITION_HIGH;
        } else {
          status = PPG_STATUS_NORMAL;
          condition = PPG_CONDITION_NORMAL;
        }

        ppg_update_condition_history(
            condition_history,
            condition);

        ppg_store_latest(
            &sample,
            bpm_average,
            status,
            ppg_is_emergency(condition_history),
            true);

        last_beat_time = current_time;
      }
    } else if (infrared_smoothed < 0.0f) {
      /*
       * Il segnale è sceso sotto la baseline:
       * il prossimo superamento della soglia sarà un nuovo picco.
       */
      peak_detected = false;
    }

    /*
     * Se non viene rilevato un battito per almeno un secondo,
     * conserva il comportamento originale e considera la
     * condizione equivalente a un battito basso.
     */
    const systime_t now = chVTGetSystemTimeX();

    const uint32_t elapsed_ms =
        chTimeI2MS(now - last_beat_time);

    if (elapsed_ms >= PPG_NO_BEAT_TIMEOUT_MS) {
      ppg_update_condition_history(
          condition_history,
          PPG_CONDITION_LOW);

      ppg_store_latest(
          &sample,
          0U,
          PPG_STATUS_NO_BEAT,
          ppg_is_emergency(condition_history),
          false);

      /*
       * Mantiene la temporizzazione originale: lo stato NO_BEAT
       * viene aggiornato al massimo una volta al secondo.
       */
      last_beat_time = now;
    }

    /* Periodo di campionamento originale. */
    chThdSleepMilliseconds(15);
  }
}

void ppg_start(void) {
  if (ppg_service_started) {
    return;
  }

  memset(&latest_ppg_data, 0, sizeof(latest_ppg_data));

  latest_ppg_update = 0U;
  latest_ppg_available = false;

  chMtxObjectInit(&latest_ppg_mutex);

  /*
   * Configura I2C1 e il MAX30102.
   *
   * Come nel progetto originale, il thread viene avviato anche
   * se una scrittura iniziale non riesce: ppg_read_sample()
   * contiene già la procedura di recovery del bus.
   */
  (void)ppg_init();

  ppg_service_started = true;

  (void)chThdCreateStatic(
      wa_ppg_thread,
      sizeof(wa_ppg_thread),
      NORMALPRIO + 1,
      ppg_thread,
      NULL);
}

bool ppg_get_latest(
    ppg_data_t *out,
    sysinterval_t max_age) {
  if ((out == NULL) || !ppg_service_started) {
    return false;
  }

  bool available = false;

  chMtxLock(&latest_ppg_mutex);

  if (latest_ppg_available) {
    const sysinterval_t age =
        chVTTimeElapsedSinceX(latest_ppg_update);

    if ((max_age == TIME_INFINITE) || (age <= max_age)) {
      *out = latest_ppg_data;
      available = true;
    }
  }

  chMtxUnlock(&latest_ppg_mutex);

  return available;
}
