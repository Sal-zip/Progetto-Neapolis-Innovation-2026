#ifndef SENSORS_MQ2_THREAD_MQ2_H_
#define SENSORS_MQ2_THREAD_MQ2_H_

#include "ch.h"
#include "mq2.h"

#include <stdbool.h>

/**
 * @brief Classificazione del valore ADC prodotto dal sensore MQ-2.
 */
typedef enum {
  MQ2_STATUS_UNKNOWN = 0,
  MQ2_STATUS_LOW,
  MQ2_STATUS_NORMAL,
  MQ2_STATUS_HIGH
} mq2_status_t;

/**
 * @brief Ultimo risultato prodotto dal thread MQ-2.
 */
typedef struct {
  /* Valore ADC grezzo acquisito da PA0/ADC1_IN1. */
  adcsample_t raw_adc;

  /* Classificazione rispetto alle soglie originali. */
  mq2_status_t status;

  /* true quando il valore è minore di 150 o maggiore di 700. */
  bool emergency;

  /* Stato corrente del buzzer. */
  bool buzzer_on;

  /* true quando la conversione ADC è riuscita. */
  bool valid;
} mq2_data_t;

/**
 * @brief Inizializza MQ-2 e buzzer e av avvia thread periodico.
 *
 *
 * Deve * Deve essere richiamata una sola volta dopo halInit() e chSysInit().
 * Eventuali chiamate successive vengono ignorate.
 */
void mq2_start(void);

/**
 * @brief Restituisce una copia sincronizzata dell'ultimo dato MQ-2.
 *
 * @param[out] out      Struttura nella quale copiare il risultato.
 * @param[in]  max_age  Età massima accettata. Usare TIME_INFINITE
 *                     per accettare qualsiasi età.
 *
 * @return true se esiste un risultato sufficientemente recente;
 *         false se non è ancora disponibile o è troppo vecchio.
 */
bool mq2_get_latest(mq2_data_t *out, sysinterval_t max_age);

#endif /* SENSORS_MQ2_THREAD_MQ2_H_ */
