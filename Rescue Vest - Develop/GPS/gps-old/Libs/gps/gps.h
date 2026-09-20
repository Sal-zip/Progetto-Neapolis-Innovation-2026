#ifndef LIBS_GPS_GPS_H_

#define LIBS_GPS_GPS_H_

#include "hal.h"
#include <stdbool.h>

#define LINE_GNSS_TX PAL_LINE(GPIOA, 9)
#define LINE_GNSS_RX PAL_LINE(GPIOA, 10)

#define GNSS_BUFFER_SIZE 256
#define MAILBOX_SIZE         4

extern msg_t rmc_mailbox_buffer[MAILBOX_SIZE];
extern mailbox_t rmc_mailbox;
extern char sentence_pool[MAILBOX_SIZE][GNSS_BUFFER_SIZE];
extern uint8_t pool_idx;

typedef struct {
    double latitude;
    double longitude;
    float speed_knots;
    float heading;
    bool is_valid;
} gps_data_t;

double convert_nmea_to_decimal(double);
float parse_float_custom(const char *);
bool parse_gprmc(const char *, gps_data_t *);
void init_gnss(void);

#endif

