#ifndef SENSORS_PPG_THREAD_PPG_H_
#define SENSORS_PPG_THREAD_PPG_H_

#include "ch.h"
#include "ppg.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Stato derivato dall'elaborazione del segnale PPG.
 */
typedef enum {
  PPG_STATUS_UNKNOWN = 0,
  PPG_STATUS_NORMAL,
  PPG_STATUS_LOW,
  PPG_STATUS_HIGH,
  PPG_STATUS_NO_BEAT
} ppg_status_t;

/**
 * @brief Ultimo risultato prodotto dal thread PPG.
 */
typedef struct {
  /* Frequenza cardiaca media calcolata sugli ultimi battiti. */
  uint32_t heart_rate_bpm;

  /* Ultimi campioni grezzi acquisiti dal MAX30102. */
  uint32_t red_raw;
  uint32_t infrared_raw;

  /* Classificazione del battito. */
  ppg_status_t status;

  /*
   * true dopo due rilevazioni consecutive di battito basso,
   * battito alto o battito assente.
   */
  bool emergency;

  /*
   * true quando heart_rate_bpm deriva da almeno un battito rilevato.
   * In caso di PPG_STATUS_NO_BEAT viene impostato a false.
   */
  bool valid;
} ppg_data_t;

/**
 * @brief Inizializza il MAX30102 e avvia il thread PPG.
 *
 * Deve essere chiamata una sola volta, dopo halInit() e chSysInit().
 * La funzione è idempotente: eventuali chiamate successive non
 * creano altri thread.
 */
void ppg_start(void);

/**
 * @brief Restituisce una copia sincronizzata dell'ultimo risultato PPG.
 *
 * Il valore di ritorno indica che esiste un risultato sufficientemente
 * recente. La validità del valore BPM è indicata separatamente dal
 * campo out->valid.
 *
 * In questo modo è possibile ottenere anche uno stato NO_BEAT,
 * che non contiene un BPM valido ma può rappresentare un'emergenza.
 *
 * @param[out] out      Struttura nella quale copiare il risultato.
 * @param[in]  max_age  Età massima accettata. Usare TIME_INFINITE
 *                     per non applicare un limite temporale.
 *
 * @return true se è disponibile un risultato recente;
 *         false se non esiste ancora un risultato o se è troppo vecchio.
 */
bool ppg_get_latest(ppg_data_t *out, sysinterval_t max_age);

#endif /* SENSORS_PPG_THREAD_PPG_H_ */
