#ifndef LIBS_GPS_GPS_H_
#define LIBS_GPS_GPS_H_

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * GPS su USART1:
 * PA9  = USART1_TX
 * PA10 = USART1_RX
 */
#define LINE_GNSS_TX PAL_LINE(GPIOA, 9U)
#define LINE_GNSS_RX PAL_LINE(GPIOA, 10U)

#define GNSS_BUFFER_SIZE 256U
#define MAILBOX_SIZE       4U

/*
 * Risorse utilizzate per trasferire le frasi NMEA
 * dal thread di ricezione al thread di elaborazione.
 */
extern msg_t rmc_mailbox_buffer[MAILBOX_SIZE];
extern mailbox_t rmc_mailbox;
extern char sentence_pool[MAILBOX_SIZE][GNSS_BUFFER_SIZE];
extern uint8_t pool_idx;

/* Informazioni GPS ricavate da una frase NMEA RMC. */
typedef struct {
  double latitude;
  double longitude;
  float speed_knots;
  float heading;
  bool is_valid;
} gps_data_t;

/* Conversione e parsing delle frasi NMEA. */
double convert_nmea_to_decimal(double nmea_value);
float parse_float_custom(const char *text);
bool parse_gprmc(const char *sentence, gps_data_t *gps_data);

/* Inizializza USART1 e le risorse del modulo GPS. */
void init_gnss(void);

#endif /* LIBS_GPS_GPS_H_ */
