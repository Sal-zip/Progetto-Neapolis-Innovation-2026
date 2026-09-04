#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "mq2bpm.h"

/* ===========================================================================
 * Shared globals
 * =========================================================================*/
BaseSequentialStream *chp = (BaseSequentialStream *)&SD2;

/* ===========================================================================
 * MAIN
 * =========================================================================*/
int main(void) {

  halInit();
  chSysInit();

  // BOSSSSSS CHIAMA QUESTO PER FAR FUNZIIONARE I SENSORIIII !!! ABBRACCI E BACIX
  init_mq2_bpm();

  while (true) {
    chThdSleepMilliseconds(1000);
  }
}
