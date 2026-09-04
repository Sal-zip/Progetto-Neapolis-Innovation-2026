#ifndef THREAD_MQ_H
#define THREAD_MQ_H

#include "ch.h"
#include "hal.h"

/* MQ-2 ADC channel */
#define MQ2_ADC_CHANNEL      ADC_CHANNEL_IN1

/* Emergency thresholds */
#define MQ2_EMERGENZA_ALTA   700
#define MQ2_EMERGENZA_BASSA  150

/* Buzzer pin */
#define BUZZER_PORT          GPIOB
#define BUZZER_PIN           4

/* Shared serial stream and mutex (defined in main.c) */
extern BaseSequentialStream *chp;
extern mutex_t serial_mtx;

/* Thread working area and function prototype */
extern THD_WORKING_AREA(wa_thread_mq2, 256);
THD_FUNCTION(thread_mq, arg);

#endif /* THREAD_MQ_H */
