/*
 * gps.c
 *
 *  Created on: 3 set 2026
 *      Author: radon
 */

#include <gps.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

msg_t rmc_mailbox_buffer[MAILBOX_SIZE];
mailbox_t rmc_mailbox;
char sentence_pool[MAILBOX_SIZE][GNSS_BUFFER_SIZE];
uint8_t pool_idx = 0;

static const SerialConfig gnsssd1cfg = {
  115200, // Baud rate Teseo-VIC3DA
  0,
  0,
  0
};

void sendgnss(const char *msg) {
  sdWrite(&SD1, (const uint8_t *)msg, strlen(msg));
  chThdSleepMilliseconds(50);
}

double convert_nmea_to_decimal(double raw_nmea) {
    int degrees = (int)(raw_nmea / 100.0);
    double minutes = raw_nmea - (degrees * 100.0);
    return degrees + (minutes / 60.0);
}

float parse_float_custom(const char *s) {
    float val = 0.0f, fact = 1.0f;
    int dot = 0;
    if (!s || *s == '\0') return 0.0f;
    if (*s == '-') { s++; fact = -1.0f; }
    while (*s) {
        if (*s == '.') { dot = 1; s++; continue; }
        if (dot) fact /= 10.0f;
        if (*s >= '0' && *s <= '9') val = val * 10.0f + (float)(*s - '0');
        s++;
    }
    return val * fact;
}

bool parse_gprmc(const char *nmea_str, gps_data_t *rmc_out) {
    const char *p = strstr(nmea_str, "RMC");
    if (!p) return false;
    p += 3; // Jump past "RMC"

    float raw_lat = 0.0f, raw_lon = 0.0f;
    char status = 'V', ns = 'N', ew = 'E';
    float speed = 0.0f, heading = 0.0f;
    int field_idx = 0;

    while (*p && *p != '*') {
        if (*p == ',') {
            field_idx++;
            p++;
            continue;
        }

        char field[16];

        size_t i = 0;
        while (*p && *p != ',' && *p != '*' && i < (sizeof(field) - 1)) {
            field[i++] = *p++;
        }
        field[i] = '\0';

        switch (field_idx) {
            case 2: status = field[0]; break; // Extract 'A' or 'V'
            case 3: raw_lat = parse_float_custom(field); break;
            case 4: ns = field[0]; break;     // Extract 'N' or 'S'
            case 5: raw_lon = parse_float_custom(field); break;
            case 6: ew = field[0]; break;     // Extract 'E' or 'W'
            case 7: speed = parse_float_custom(field); break;
            case 8: heading = parse_float_custom(field); break;
            default: break;
        }
    }

    if (status != 'A' || raw_lat == 0.0f || raw_lon == 0.0f) {
        rmc_out->is_valid = false;
        return false;
    }

    rmc_out->latitude = convert_nmea_to_decimal(raw_lat);
    if (ns == 'S') rmc_out->latitude = -rmc_out->latitude;

    rmc_out->longitude = convert_nmea_to_decimal(raw_lon);
    if (ew == 'W') rmc_out->longitude = -rmc_out->longitude;

    rmc_out->speed_knots = speed;
    rmc_out->heading = heading;
    rmc_out->is_valid = true;

    return true;
}

void init_gnss(void) {
  palSetLineMode(LINE_GNSS_TX, PAL_MODE_ALTERNATE(7) | PAL_MODE_OUTPUT_PUSHPULL); //TX
  palSetLineMode(LINE_GNSS_RX, PAL_MODE_INPUT_PULLUP | PAL_MODE_ALTERNATE(7)); //RX
  sdStart(&SD1, &gnsssd1cfg);

  chMBObjectInit(&rmc_mailbox, rmc_mailbox_buffer, MAILBOX_SIZE);

  sendgnss("$PSTMCFGSETFIXRATE,1,1000*5F\r\n");
  sendgnss("$PSTMCFGMSGL,3,1,1,0*1D\r\n");
  sendgnss("$PSTMSAVEPAR*5C\r\n");
}
