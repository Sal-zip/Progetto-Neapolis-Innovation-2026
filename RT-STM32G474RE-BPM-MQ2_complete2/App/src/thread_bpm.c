#include "thread_bpm.h"
#include "chprintf.h"
#include <stdbool.h>

/* ===========================================================================
 * I2C Configuration
 * =========================================================================*/
const I2CConfig i2ccfg = {
  .timingr = 0x20303E5D
};

/* ===========================================================================
 * I2C Bus Recovery
 * =========================================================================*/
static void i2c_bus_recovery(void) {
  palSetPadMode(GPIOB, 8, PAL_MODE_OUTPUT_OPENDRAIN |
                          PAL_STM32_OSPEED_HIGHEST  |
                          PAL_STM32_PUPDR_PULLUP);
  for (int i = 0; i < 9; i++) {
    palClearPad(GPIOB, 8);
    chThdSleepMilliseconds(1);
    palSetPad(GPIOB, 8);
    chThdSleepMilliseconds(1);
  }
  palSetPadMode(GPIOB, 9, PAL_MODE_OUTPUT_OPENDRAIN |
                          PAL_STM32_OSPEED_HIGHEST  |
                          PAL_STM32_PUPDR_PULLUP);
  palClearPad(GPIOB, 9);
  chThdSleepMilliseconds(1);
  palSetPad(GPIOB, 9);
  chThdSleepMilliseconds(1);
  palSetPadMode(GPIOB, 8, PAL_MODE_ALTERNATE(4) |
                          PAL_STM32_OTYPE_OPENDRAIN |
                          PAL_STM32_OSPEED_HIGHEST  |
                          PAL_STM32_PUPDR_PULLUP);
  palSetPadMode(GPIOB, 9, PAL_MODE_ALTERNATE(4) |
                          PAL_STM32_OTYPE_OPENDRAIN |
                          PAL_STM32_OSPEED_HIGHEST  |
                          PAL_STM32_PUPDR_PULLUP);
}

static void i2c_reset_periph(void) {
  i2cStop(&I2CD1);
  chThdSleepMilliseconds(10);
  i2cStart(&I2CD1, &i2ccfg);
  chThdSleepMilliseconds(10);
}

/* ===========================================================================
 * MAX30102 Helpers
 * =========================================================================*/
static bool max30102_write_reg(uint8_t reg, uint8_t val) {
  uint8_t txbuf[2] = {reg, val};
  i2cAcquireBus(&I2CD1);
  msg_t status = i2cMasterTransmitTimeout(&I2CD1, MAX30102_ADDR, txbuf, 2, NULL, 0, TIME_MS2I(100));
  i2cReleaseBus(&I2CD1);
  if (status != MSG_OK) {
    i2c_bus_recovery();
    i2c_reset_periph();
    return false;
  }
  return true;
}

static void max30102_reset_fifo(void) {
  max30102_write_reg(REG_FIFO_WR_PTR,      0x00);
  max30102_write_reg(REG_FIFO_OVF_COUNTER, 0x00);
  max30102_write_reg(REG_FIFO_RD_PTR,      0x00);
}

/* ===========================================================================
 * Thread BPM
 * =========================================================================*/
THD_WORKING_AREA(wa_thread_bpm, 512);

THD_FUNCTION(thread_bpm, arg) {
  (void)arg;
  chRegSetThreadName("bpm");

  /* Init MAX30102 registers */
  max30102_write_reg(REG_MODE_CONFIG, 0x03);
  chThdSleepMilliseconds(10);
  max30102_write_reg(REG_SPO2_CONFIG, 0x27);
  chThdSleepMilliseconds(10);
  max30102_write_reg(REG_LED1_PA, 0x24);
  max30102_write_reg(REG_LED2_PA, 0x24);
  chThdSleepMilliseconds(10);
  max30102_reset_fifo();

  /* Filter and BPM state */
  float ir_dc_estimator = 0.0f;
  float ir_ac_signal = 0.0f;
  float ir_smoothed = 0.0f;
  bool peak_detected = false;
  systime_t last_beat_time = chVTGetSystemTime();
  bool first_sample = true;

  #define BPM_AVG_WINDOW 8
  uint32_t bpm_history[BPM_AVG_WINDOW];
  uint8_t  bpm_index = 0;
  uint8_t  bpm_count = 0;
  uint32_t bpm_avg = 0;

  uint8_t last_two_conditions[2] = {0, 0};
  bool emergenza_basso_inviata = false;
  bool emergenza_alto_inviata = false;
  #define COND_NORMALE  0
  #define COND_BASSO    1
  #define COND_ALTO     2
  #define MIN_BEAT_INTERVAL_MS  300

  while (true) {
    uint8_t reg_addr = REG_FIFO_DATA;
    uint8_t rxbuf[6] = {0};

    i2cAcquireBus(&I2CD1);
    msg_t status = i2cMasterTransmitTimeout(&I2CD1, MAX30102_ADDR,
                                            &reg_addr, 1,
                                            rxbuf, 6,
                                            TIME_MS2I(50));
    i2cReleaseBus(&I2CD1);

    if (status != MSG_OK) {
      i2c_bus_recovery();
      i2c_reset_periph();
      i2cAcquireBus(&I2CD1);
      status = i2cMasterTransmitTimeout(&I2CD1, MAX30102_ADDR,
                                        &reg_addr, 1,
                                        rxbuf, 6,
                                        TIME_MS2I(50));
      i2cReleaseBus(&I2CD1);
    }

    if (status == MSG_OK) {
      uint32_t ir_val = (((uint32_t)rxbuf[3] << 16) |
                         ((uint32_t)rxbuf[4] << 8)  |
                          (uint32_t)rxbuf[5]) & 0x03FFFF;

      if (first_sample) {
        ir_dc_estimator = (float)ir_val;
        first_sample = false;
        last_beat_time = chVTGetSystemTime();
      }

      /* DC removal */
      ir_dc_estimator = (ir_dc_estimator * 0.95f) + ((float)ir_val * 0.05f);
      ir_ac_signal = (float)ir_val - ir_dc_estimator;

      /* Low-pass filter */
      ir_smoothed = (ir_smoothed * 0.8f) + (ir_ac_signal * 0.2f);

      /* Peak Detection */
      if (ir_smoothed > 20.0f && !peak_detected) {
        systime_t current_time = chVTGetSystemTime();
        uint32_t delta_ms = chTimeI2MS(current_time - last_beat_time);

        if (delta_ms > MIN_BEAT_INTERVAL_MS) {
          peak_detected = true;
          uint32_t bpm = 60000 / delta_ms;

          bpm_history[bpm_index] = bpm;
          bpm_index = (bpm_index + 1) % BPM_AVG_WINDOW;
          if (bpm_count < BPM_AVG_WINDOW) bpm_count++;

          bpm_avg = 0;
          for (uint8_t i = 0; i < bpm_count; i++) bpm_avg += bpm_history[i];
          bpm_avg /= bpm_count;

          //chMtxLock(&serial_mtx);
          if (bpm_avg < 40) {
            //chprintf(chp, "bpm: %lu | battito basso\r\n", bpm_avg);
            last_two_conditions[0] = last_two_conditions[1];
            last_two_conditions[1] = COND_BASSO;
          } else if (bpm_avg > 120) {
            //chprintf(chp, "bpm: %lu | battito alto\r\n", bpm_avg);
            last_two_conditions[0] = last_two_conditions[1];
            last_two_conditions[1] = COND_ALTO;
          } else {
            //chprintf(chp, "bpm: %lu | battito normale\r\n", bpm_avg);
            last_two_conditions[0] = last_two_conditions[1];
            last_two_conditions[1] = COND_NORMALE;
          }
          //chMtxUnlock(&serial_mtx);

          last_beat_time = current_time;
        }
      }
      else if (ir_smoothed < 0.0f) {
        peak_detected = false;
      }

      /* No beat (> 1 second) — report every second */
      systime_t now = chVTGetSystemTime();
      uint32_t elapsed_ms = chTimeI2MS(now - last_beat_time);
      if (elapsed_ms >= 1000) {
        //chMtxLock(&serial_mtx);
        //chprintf(chp, "bpm: 0 | battito assente\r\n");
        //chMtxUnlock(&serial_mtx);
        last_two_conditions[0] = last_two_conditions[1];
        last_two_conditions[1] = COND_BASSO;
        last_beat_time = now;
      }

      /* Emergency low beat */
      if (last_two_conditions[0] == COND_BASSO && last_two_conditions[1] == COND_BASSO) {
        if (!emergenza_basso_inviata) {
          //chMtxLock(&serial_mtx);
          //chprintf(chp, "EMERGENZA BATTITO BASSO\r\n");
          //chMtxUnlock(&serial_mtx);
          emergenza_basso_inviata = true;
        }
      } else {
        emergenza_basso_inviata = false;
      }

      /* Emergency high beat */
      if (last_two_conditions[0] == COND_ALTO && last_two_conditions[1] == COND_ALTO) {
        if (!emergenza_alto_inviata) {
          //chMtxLock(&serial_mtx);
          //chprintf(chp, "EMERGENZA BATTITO ALTO\r\n");
          //chMtxUnlock(&serial_mtx);
          emergenza_alto_inviata = true;
        }
      } else {
        emergenza_alto_inviata = false;
      }
    }

    chThdSleepMilliseconds(15);
  }
}
