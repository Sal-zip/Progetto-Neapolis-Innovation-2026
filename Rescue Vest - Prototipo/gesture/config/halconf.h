#ifndef HALCONF_H
#define HALCONF_H

#define _CHIBIOS_HAL_CONF_
#define _CHIBIOS_HAL_CONF_VER_8_4_

#include "mcuconf.h"

/*===========================================================================*/
/* HAL driver activation.                                                    */
/*===========================================================================*/

#if !defined(HAL_USE_PAL) || defined(__DOXYGEN__)
#define HAL_USE_PAL                         TRUE
#endif

#if !defined(HAL_USE_ADC) || defined(__DOXYGEN__)
#define HAL_USE_ADC                         FALSE
#endif

#if !defined(HAL_USE_CAN) || defined(__DOXYGEN__)
#define HAL_USE_CAN                         FALSE
#endif

#if !defined(HAL_USE_CRY) || defined(__DOXYGEN__)
#define HAL_USE_CRY                         FALSE
#endif

#if !defined(HAL_USE_DAC) || defined(__DOXYGEN__)
#define HAL_USE_DAC                         FALSE
#endif

#if !defined(HAL_USE_EFL) || defined(__DOXYGEN__)
#define HAL_USE_EFL                         FALSE
#endif

#if !defined(HAL_USE_GPT) || defined(__DOXYGEN__)
#define HAL_USE_GPT                         FALSE
#endif

#if !defined(HAL_USE_I2C) || defined(__DOXYGEN__)
#define HAL_USE_I2C                         TRUE
#endif

#if !defined(HAL_USE_I2S) || defined(__DOXYGEN__)
#define HAL_USE_I2S                         FALSE
#endif

#if !defined(HAL_USE_ICU) || defined(__DOXYGEN__)
#define HAL_USE_ICU                         FALSE
#endif

#if !defined(HAL_USE_MAC) || defined(__DOXYGEN__)
#define HAL_USE_MAC                         FALSE
#endif

#if !defined(HAL_USE_MMC_SPI) || defined(__DOXYGEN__)
#define HAL_USE_MMC_SPI                     FALSE
#endif

#if !defined(HAL_USE_PWM) || defined(__DOXYGEN__)
#define HAL_USE_PWM                         TRUE
#endif

#if !defined(HAL_USE_RTC) || defined(__DOXYGEN__)
#define HAL_USE_RTC                         FALSE
#endif

#if !defined(HAL_USE_SDC) || defined(__DOXYGEN__)
#define HAL_USE_SDC                         FALSE
#endif

#if !defined(HAL_USE_SERIAL) || defined(__DOXYGEN__)
#define HAL_USE_SERIAL                      TRUE
#endif

#if !defined(HAL_USE_SERIAL_USB) || defined(__DOXYGEN__)
#define HAL_USE_SERIAL_USB                  FALSE
#endif

#if !defined(HAL_USE_SIO) || defined(__DOXYGEN__)
#define HAL_USE_SIO                         TRUE
#endif

#if !defined(HAL_USE_SPI) || defined(__DOXYGEN__)
#define HAL_USE_SPI                         FALSE
#endif

#if !defined(HAL_USE_TRNG) || defined(__DOXYGEN__)
#define HAL_USE_TRNG                        FALSE
#endif

#if !defined(HAL_USE_UART) || defined(__DOXYGEN__)
#define HAL_USE_UART                        FALSE
#endif

#if !defined(HAL_USE_USB) || defined(__DOXYGEN__)
#define HAL_USE_USB                         FALSE
#endif

#if !defined(HAL_USE_WDG) || defined(__DOXYGEN__)
#define HAL_USE_WDG                         FALSE
#endif

#if !defined(HAL_USE_WSPI) || defined(__DOXYGEN__)
#define HAL_USE_WSPI                        FALSE
#endif

/*===========================================================================*/
/* PAL settings.                                                             */
/*===========================================================================*/

#if !defined(PAL_USE_CALLBACKS) || defined(__DOXYGEN__)
#define PAL_USE_CALLBACKS                   TRUE
#endif

#if !defined(PAL_USE_WAIT) || defined(__DOXYGEN__)
#define PAL_USE_WAIT                        TRUE
#endif

/*===========================================================================*/
/* ADC settings.                                                             */
/*===========================================================================*/

#if !defined(ADC_USE_WAIT) || defined(__DOXYGEN__)
#define ADC_USE_WAIT                        TRUE
#endif

#if !defined(ADC_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define ADC_USE_MUTUAL_EXCLUSION            TRUE
#endif

/*===========================================================================*/
/* CAN settings.                                                             */
/*===========================================================================*/

#if !defined(CAN_USE_SLEEP_MODE) || defined(__DOXYGEN__)
#define CAN_USE_SLEEP_MODE                  TRUE
#endif

#if !defined(CAN_ENFORCE_USE_CALLBACKS) || defined(__DOXYGEN__)
#define CAN_ENFORCE_USE_CALLBACKS           FALSE
#endif

/*===========================================================================*/
/* Cryptography settings.                                                    */
/*===========================================================================*/

#if !defined(HAL_CRY_USE_FALLBACK) || defined(__DOXYGEN__)
#define HAL_CRY_USE_FALLBACK                FALSE
#endif

#if !defined(HAL_CRY_ENFORCE_FALLBACK) || defined(__DOXYGEN__)
#define HAL_CRY_ENFORCE_FALLBACK            FALSE
#endif

/*===========================================================================*/
/* DAC settings.                                                             */
/*===========================================================================*/

#if !defined(DAC_USE_WAIT) || defined(__DOXYGEN__)
#define DAC_USE_WAIT                        TRUE
#endif

#if !defined(DAC_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define DAC_USE_MUTUAL_EXCLUSION            TRUE
#endif

/*===========================================================================*/
/* I2C settings.                                                             */
/*===========================================================================*/

#if !defined(I2C_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define I2C_USE_MUTUAL_EXCLUSION            TRUE
#endif

/*===========================================================================*/
/* MAC settings.                                                             */
/*===========================================================================*/

#if !defined(MAC_USE_ZERO_COPY) || defined(__DOXYGEN__)
#define MAC_USE_ZERO_COPY                   FALSE
#endif

#if !defined(MAC_USE_EVENTS) || defined(__DOXYGEN__)
#define MAC_USE_EVENTS                      TRUE
#endif

/*===========================================================================*/
/* MMC/SPI settings.                                                         */
/*===========================================================================*/

#if !defined(MMC_NICE_WAITING) || defined(__DOXYGEN__)
#define MMC_NICE_WAITING                    TRUE
#endif

/*===========================================================================*/
/* SDC settings.                                                             */
/*===========================================================================*/

#if !defined(SDC_INIT_RETRY) || defined(__DOXYGEN__)
#define SDC_INIT_RETRY                      100
#endif

#if !defined(SDC_MMC_SUPPORT) || defined(__DOXYGEN__)
#define SDC_MMC_SUPPORT                     FALSE
#endif

#if !defined(SDC_NICE_WAITING) || defined(__DOXYGEN__)
#define SDC_NICE_WAITING                    TRUE
#endif

#if !defined(SDC_INIT_OCR_V20) || defined(__DOXYGEN__)
#define SDC_INIT_OCR_V20                    0x50FF8000U
#endif

#if !defined(SDC_INIT_OCR) || defined(__DOXYGEN__)
#define SDC_INIT_OCR                        0x80100000U
#endif

/*===========================================================================*/
/* Serial settings.                                                          */
/*===========================================================================*/

#if !defined(SERIAL_DEFAULT_BITRATE) || defined(__DOXYGEN__)
#define SERIAL_DEFAULT_BITRATE              38400
#endif

#if !defined(SERIAL_BUFFERS_SIZE) || defined(__DOXYGEN__)
#define SERIAL_BUFFERS_SIZE                 16
#endif

/*===========================================================================*/
/* SIO settings: LPUART1 verso la board principale.                          */
/*===========================================================================*/

#if !defined(SIO_DEFAULT_BITRATE) || defined(__DOXYGEN__)
#define SIO_DEFAULT_BITRATE                 38400
#endif

#if !defined(SIO_USE_SYNCHRONIZATION) || defined(__DOXYGEN__)
#define SIO_USE_SYNCHRONIZATION             TRUE
#endif

/*===========================================================================*/
/* Serial-over-USB settings.                                                 */
/*===========================================================================*/

#if !defined(SERIAL_USB_BUFFERS_SIZE) || defined(__DOXYGEN__)
#define SERIAL_USB_BUFFERS_SIZE             256
#endif

#if !defined(SERIAL_USB_BUFFERS_NUMBER) || defined(__DOXYGEN__)
#define SERIAL_USB_BUFFERS_NUMBER           2
#endif

/*===========================================================================*/
/* SPI settings.                                                             */
/*===========================================================================*/

#if !defined(SPI_USE_WAIT) || defined(__DOXYGEN__)
#define SPI_USE_WAIT                        TRUE
#endif

#if !defined(SPI_USE_ASSERT_ON_ERROR) || defined(__DOXYGEN__)
#define SPI_USE_ASSERT_ON_ERROR             TRUE
#endif

#if !defined(SPI_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define SPI_USE_MUTUAL_EXCLUSION            TRUE
#endif

#if !defined(SPI_SELECT_MODE) || defined(__DOXYGEN__)
#define SPI_SELECT_MODE                     SPI_SELECT_MODE_PAD
#endif

/*===========================================================================*/
/* UART settings.                                                            */
/*===========================================================================*/

#if !defined(UART_USE_WAIT) || defined(__DOXYGEN__)
#define UART_USE_WAIT                       FALSE
#endif

#if !defined(UART_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define UART_USE_MUTUAL_EXCLUSION           FALSE
#endif

/*===========================================================================*/
/* USB settings.                                                             */
/*===========================================================================*/

#if !defined(USB_USE_WAIT) || defined(__DOXYGEN__)
#define USB_USE_WAIT                        FALSE
#endif

/*===========================================================================*/
/* WSPI settings.                                                            */
/*===========================================================================*/

#if !defined(WSPI_USE_WAIT) || defined(__DOXYGEN__)
#define WSPI_USE_WAIT                       TRUE
#endif

#if !defined(WSPI_USE_MUTUAL_EXCLUSION) || defined(__DOXYGEN__)
#define WSPI_USE_MUTUAL_EXCLUSION           TRUE
#endif

#endif /* HALCONF_H */
