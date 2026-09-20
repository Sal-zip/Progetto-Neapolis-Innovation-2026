#include "ch.h"
#include "hal.h"

#include "application.h"

int main(void) {
  halInit();
  chSysInit();

  application_start();

  while (true) {
    chThdSleepMilliseconds(1000);
  }
}
