#include "mq2bpm.h"
#include "thread_bpm.h"
#include "thread_mq.h"

void init_mq2_bpm() {
  /* 2. ADC1 for MQ-2 gas sensor (PA0) */
  palSetPadMode(GPIOA, 4, PAL_MODE_INPUT_ANALOG);
  adcStart(&ADCD1, NULL);

  /* 3. I2C1 for MAX30102 (PB8=SCL, PB9=SDA) */
  palSetPadMode(GPIOB, 8, PAL_MODE_ALTERNATE(4) |
                          PAL_STM32_OTYPE_OPENDRAIN |
                          PAL_STM32_OSPEED_HIGHEST  |
                          PAL_STM32_PUPDR_PULLUP);
  palSetPadMode(GPIOB, 9, PAL_MODE_ALTERNATE(4) |
                          PAL_STM32_OTYPE_OPENDRAIN |
                          PAL_STM32_OSPEED_HIGHEST  |
                          PAL_STM32_PUPDR_PULLUP);
  i2cStart(&I2CD1, &i2ccfg);
  chThdSleepMilliseconds(100);

  /* 4. Buzzer pin (PB4 = TIM3_CH1) */
  palSetPadMode(GPIOB, 4, PAL_MODE_ALTERNATE(2));

  /* 5. Create threads */
  chThdCreateStatic(wa_thread_bpm, sizeof(wa_thread_bpm),
                    NORMALPRIO + 1, thread_bpm, NULL);
  chThdCreateStatic(wa_thread_mq2, sizeof(wa_thread_mq2),
                    NORMALPRIO, thread_mq, NULL);
}

