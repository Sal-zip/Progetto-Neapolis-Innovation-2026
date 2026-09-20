#include "ch.h"
#include "hal.h"

#include "gesture_application.h"

int main(void) {

  /*
   * Inizializzazione HAL e kernel ChibiOS.
   * Deve precedere l'avvio di qualsiasi driver o thread.
   */
  halInit();
  chSysInit();

  /*
   * Avvia:
   *   - seriale di debug;
   *   - buzzer;
   *   - comunicazione con la board principale;
   *   - sensore VL53L7CX;
   *   - gestione applicativa delle gesture.
   */
  gesture_application_start();

  /*
   * Il lavoro viene svolto dai thread dei singoli moduli.
   * Il main rimane attivo senza effettuare polling.
   */
  while (true) {
    chThdSleepMilliseconds(1000U);
  }
}
