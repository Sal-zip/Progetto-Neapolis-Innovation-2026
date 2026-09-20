#ifndef SENSORS_MQ2_MQ2_H_
#define SENSORS_MQ2_MQ2_H_

#include "ch.h"
#include "hal.h"

#include <stdbool.h>

/**
 * @brief Campione grezzo acquisito dal sensore MQ-2.
 *
 * Il valore è una misura ADC non calibrata. Non rappresenta
 * direttamente una concentrazione espressa in ppm.
 */
typedef struct {
  adcsample_t raw_adc;
} mq2_sample_t;

/**
 * @brief Inizializza ADC1 e configura PA0 come ingresso analogico.
 *
 * Questa funzione verrà richiamata internamente da mq2_start().
 *
 * @return true quando l'inizializzazione è stata completata.
 */
bool mq2_init(void);

/**
 * @brief Acquisisce un campione dal sensore MQ-2.
 *
 * @param[out] sample Struttura nella quale salvare il valore ADC.
 *
 * @return true se la conversione è terminata correttamente;
 *         false in caso di errore o parametro non valido.
 */
bool mq2_read_sample(mq2_sample_t *sample);

#endif /* SENSORS_MQ2_MQ2_H_ */
