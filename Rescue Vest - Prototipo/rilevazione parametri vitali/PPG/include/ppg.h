#ifndef SENSORS_PPG_PPG_H_
#define SENSORS_PPG_PPG_H_

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Campione grezzo acquisito dal MAX30102.
 *
 * In modalità SpO2 il sensore inserisce nella FIFO:
 * - tre byte per il LED rosso;
 * - tre byte per il LED infrarosso.
 *
 * Entrambi i valori sono codificati su 18 bit.
 */
typedef struct {
  uint32_t red;
  uint32_t infrared;
} ppg_sample_t;

/**
 * @brief Inizializza I2C1 e configura il MAX30102.
 *
 * Configura:
 * - PB8 come I2C1_SCL;
 * - PB9 come I2C1_SDA;
 * - MAX30102 in modalità SpO2;
 * - intensità dei LED;
 * - FIFO del sensore.
 *
 * Questa funzione verrà richiamata internamente da ppg_start().
 *
 * @return true se tutti i registri sono stati configurati correttamente;
 *         false se si è verificato un errore I2C.
 */
bool ppg_init(void);

/**
 * @brief Legge un campione dalla FIFO del MAX30102.
 *
 * In caso di errore I2C viene effettuato un tentativo di recupero
 * del bus e la lettura viene ripetuta una volta.
 *
 * @param[out] sample Destinazione del campione acquisito.
 *
 * @return true se la lettura è riuscita; false in caso contrario.
 */
bool ppg_read_sample(ppg_sample_t *sample);

#endif /* SENSORS_PPG_PPG_H_ */
