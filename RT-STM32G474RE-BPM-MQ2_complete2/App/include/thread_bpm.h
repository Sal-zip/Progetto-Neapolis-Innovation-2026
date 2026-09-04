#ifndef THREAD_BPM_H
#define THREAD_BPM_H

#include "ch.h"
#include "hal.h"

/* MAX30102 I2C address */
#define MAX30102_ADDR        0x57

/* MAX30102 registers */
#define REG_FIFO_WR_PTR      0x02
#define REG_FIFO_OVF_COUNTER 0x03
#define REG_FIFO_RD_PTR      0x04
#define REG_FIFO_DATA        0x07
#define REG_MODE_CONFIG      0x09
#define REG_SPO2_CONFIG      0x0A
#define REG_LED1_PA          0x0C
#define REG_LED2_PA          0x0D

/* I2C configuration (100kHz, PCLK1=85MHz) */
extern const I2CConfig i2ccfg;

/* Shared serial stream and mutex (defined in main.c) */
extern BaseSequentialStream *chp;

/* Thread working area and function prototype */
extern THD_WORKING_AREA(wa_thread_bpm, 512);
THD_FUNCTION(thread_bpm, arg);

#endif /* THREAD_BPM_H */
