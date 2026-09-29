#ifndef THREAD_BPM_H
#define THREAD_BPM_H

#include "ch.h"
#include "hal.h"

typedef struct {
  uint16_t raw; /**< Valore grezzo ADC a 12 bit (da 0 a 4095) */
} ppg_sample_t;

/**
 * @brief Inizializza il pin PA0 come ingresso analogico e avvia ADC1.
 * @return true se l'inizializzazione è andata a buon fine.
 */
bool ppg_init(void);

/**
 * @brief Legge un campione grezzo dall'ADC1.
 * @param[out] sample Destinazione del valore acquisito.
 * @return true se la conversione è riuscita.
 */
bool ppg_read_sample(ppg_sample_t *sample);

/** @brief Avvia il thread ChibiOS per il campionamento del battito. */
void bpm_thread_start(void);

/** @brief Restituisce l'ultimo valore di BPM calcolato (0 se dito assente). */
uint16_t bpm_get_value(void);

/** @brief Restituisce l'ultimo valore RAW letto dall'ADC. */
uint16_t bpm_get_raw(void);

#endif /* THREAD_BPM_H */
