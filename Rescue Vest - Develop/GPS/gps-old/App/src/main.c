/*
 * Thread 1: BPM da MAX30102 (I2C, polling ogni 15ms)
 * Thread 2: MQ-2 ADC + Buzzer (ogni 1 secondo)
 */
#include "ch.h"
#include "hal.h"
#include "thread_gps.h"

int main(void) {
  halInit();
  chSysInit();

  init_gnss();
  gps_data_t mgps = {0};
  thread_start_gps(&mgps);

  while (true) {
    chThdSleepMilliseconds(1000);
  }
}
