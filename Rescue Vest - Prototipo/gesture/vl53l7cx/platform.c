/**
 *
 * Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "platform.h"
#include "ch.h"
#include "hal.h"

uint8_t VL53L7CX_RdByte(VL53L7CX_Platform *p_platform, uint16_t RegisterAddress,
                        uint8_t *p_value) {
  uint8_t status = 0;
  uint8_t data_write[2];

  data_write[0] = (RegisterAddress >> 8) & 0xFF;
  data_write[1] = RegisterAddress & 0xFF;

#if VL53L7CX_SHARED_I2C
  i2cAcquireBus(p_platform->i2cp);
#endif /* VL53L7CX_SHARED_I2C */

  i2cStart(p_platform->i2cp, p_platform->i2ccfg);

  status = i2cMasterTransmitTimeout(p_platform->i2cp, p_platform->address,
                                    data_write, 2, p_value, 1, TIME_INFINITE);
#if VL53L7CX_SHARED_I2C
  i2cReleaseBus(p_platform->i2cp);
#endif /* VL53L7CX_SHARED_I2C */

  return status;
}

uint8_t VL53L7CX_WrByte(VL53L7CX_Platform *p_platform, uint16_t RegisterAddress,
                        uint8_t value) {
  uint8_t data_write[3];
  uint8_t status = 0;
  data_write[0] = (RegisterAddress >> 8) & 0xFF;
  data_write[1] = RegisterAddress & 0xFF;
  data_write[2] = value & 0xFF;
#if VL53L7CX_SHARED_I2C
  i2cAcquireBus(p_platform->i2cp);
#endif /* VL53L7CX_SHARED_I2C */

  i2cStart(p_platform->i2cp, p_platform->i2ccfg);

  status = i2cMasterTransmitTimeout(p_platform->i2cp, p_platform->address,
                                    data_write, 3, NULL, 0, TIME_INFINITE);
#if VL53L7CX_SHARED_I2C
  i2cReleaseBus(p_platform->i2cp);
#endif /* VL53L7CX_SHARED_I2C */

  return status;
}

uint8_t VL53L7CX_WrMulti(VL53L7CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t *p_values,
                         uint32_t size) {

  uint8_t okay = 1;
  uint32_t written = 0;

  while (written < size) {
    uint32_t chunk = size - written;
    if (chunk > CHUNK_SIZE)
      chunk = CHUNK_SIZE;

    okay &= VL53L7CX_WrChunk(p_platform, RegisterAddress + written,
                             p_values + written, chunk);

    written += chunk;
  }
  return okay;
}

uint8_t VL53L7CX_WrChunk(VL53L7CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t *p_values,
                         uint32_t size) {

  uint8_t data_write[CHUNK_SIZE + 2];
  uint8_t status = 0;

  osalDbgCheck(size <= CHUNK_SIZE);

  data_write[0] = (RegisterAddress >> 8) & 0xFF;
  data_write[1] = RegisterAddress & 0xFF;
  for (size_t i = 0; i < size; i++) {
    data_write[2 + i] = p_values[i];
  }
#if VL53L7CX_SHARED_I2C
  i2cAcquireBus(p_platform->i2cp);
#endif /* VL53L7CX_SHARED_I2C */

  i2cStart(p_platform->i2cp, p_platform->i2ccfg);

  status = i2cMasterTransmitTimeout(p_platform->i2cp, p_platform->address,
                                    data_write, 2 + size, NULL, 0,
                                    TIME_INFINITE);
#if VL53L7CX_SHARED_I2C
  i2cReleaseBus(p_platform->i2cp);
#endif /* VL53L7CX_SHARED_I2C */

  return status;
}

uint8_t VL53L7CX_RdMulti(VL53L7CX_Platform *p_platform,
                         uint16_t RegisterAddress, uint8_t *p_values,
                         uint32_t size) {

  uint8_t status = 0;
  uint8_t data_write[2];

  data_write[0] = (RegisterAddress >> 8) & 0xFF;
  data_write[1] = RegisterAddress & 0xFF;

#if VL53L7CX_SHARED_I2C
    i2cAcquireBus(p_platform->i2cp);
  #endif /* VL53L7CX_SHARED_I2C */

  i2cStart(p_platform->i2cp, p_platform->i2ccfg);

  status = i2cMasterTransmitTimeout(p_platform->i2cp, p_platform->address,
                                    data_write, 2, p_values, size,
                                    TIME_INFINITE);
#if VL53L7CX_SHARED_I2C
    i2cReleaseBus(p_platform->i2cp);
  #endif /* VL53L7CX_SHARED_I2C */

  return status;

}

//uint8_t VL53L7CX_WrMulti(VL53L7CX_Platform *p_platform, uint16_t RegisterAddress,
//                         uint8_t *p_values, uint32_t size) {
//
//  uint8_t okay = 1;
//    for (size_t w_add_i = RegisterAddress; w_add_i < RegisterAddress + size;
//        w_add_i++) {
//      okay &= VL53L7CX_WrByte(p_platform, w_add_i,
//                              *(p_values + w_add_i - RegisterAddress));
//    }
//    return okay;
//}

uint8_t VL53L7CX_Reset_Sensor(VL53L7CX_Platform *p_platform) {
  uint8_t status = 0;

  /* (Optional) Need to be implemented by customer. This function returns 0 if OK */

  /* Set pin LPN to LOW */
  /* Set pin AVDD to LOW */
  /* Set pin VDDIO  to LOW */
  VL53L7CX_WaitMs(p_platform, 100);

  /* Set pin LPN of to HIGH */
  /* Set pin AVDD of to HIGH */
  /* Set pin VDDIO of  to HIGH */
  VL53L7CX_WaitMs(p_platform, 100);

  return status;
}

void VL53L7CX_SwapBuffer(uint8_t *buffer, uint16_t size) {
  uint32_t i, tmp;

  /* Example of possible implementation using <string.h> */
  for (i = 0; i < size; i = i + 4) {
    tmp = (buffer[i] << 24) | (buffer[i + 1] << 16) | (buffer[i + 2] << 8)
        | (buffer[i + 3]);

    memcpy(&(buffer[i]), &tmp, 4);
  }
}

uint8_t VL53L7CX_WaitMs(VL53L7CX_Platform *p_platform, uint32_t TimeMs) {

  (void)p_platform;
  chThdSleepMilliseconds(TimeMs);
  return 0;
}
