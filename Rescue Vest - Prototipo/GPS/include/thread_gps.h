#ifndef SENSORS_GPS_THREAD_GPS_H_
#define SENSORS_GPS_THREAD_GPS_H_

#include "ch.h"
#include "gps.h"

#include <stdbool.h>

/**
 * @brief Avvia il modulo GPS e i relativi thread.
 *
 * Inizializza SD1 tramite init_gnss(), quindi avvia:
 * - il thread di ricezione delle frasi NMEA;
 * - il thread di parsing delle frasi RMC.
 *
 * Deve essere chiamata una sola volta, dopo halInit() e chSysInit().
 */
void gps_start(void);

/**
 * @brief Restituisce una copia sincronizzata dell'ultima posizione valida.
 *
 * @param[out] out      Struttura nella quale copiare i dati GPS.
 * @param[in]  max_age  Età massima accettata per il dato.
 *                     Usare TIME_INFINITE per accettare qualsiasi età.
 *
 * @return true se è disponibile una posizione valida e sufficientemente
 *         recente; false in caso contrario.
 */
bool gps_get_latest(gps_data_t *out, sysinterval_t max_age);

#endif /* SENSORS_GPS_THREAD_GPS_H_ */
