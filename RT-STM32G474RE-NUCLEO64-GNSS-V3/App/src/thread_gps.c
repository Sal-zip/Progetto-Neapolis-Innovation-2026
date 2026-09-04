#include "thread_gps.h"
#include "gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chprintf.h"
#include <stdbool.h>

static gps_data_t* gps_data;
static BaseSequentialStream *chp = (BaseSequentialStream*) &SD2;

THD_WORKING_AREA(waGNSS_Rx, 512);
THD_WORKING_AREA(waGNSS_Parser, 1024);
THD_FUNCTION(thd_gnss_receive, arg) {
  (void)arg;
  chRegSetThreadName("GNSS");

  char rx_buffer[GNSS_BUFFER_SIZE];
  int rx_idx = 0;
  bool frame_started = false;

  while (true) {
    // High priority thread sleeps until a character drops into the hardware registers
    msg_t c = sdGetTimeout(&SD1, TIME_INFINITE);

    if (c != MSG_RESET && c != MSG_TIMEOUT) {
      char character = (char)c;

      if (character == '$') {
         rx_idx = 0;
         rx_buffer[rx_idx++] = character;
         frame_started = true;
      }
      else if (frame_started && (character == '\n' || character == '\r')) {
         if (rx_idx > 10) {
           rx_buffer[rx_idx] = '\0';

           // Instantly shift the frame string array into the buffer pool
           strncpy(sentence_pool[pool_idx], rx_buffer, GNSS_BUFFER_SIZE - 1);
           sentence_pool[pool_idx][GNSS_BUFFER_SIZE - 1] = '\0';

           // Ship the data memory pointer to the worker thread via a Mailbox token
           chMBPostTimeout(&rmc_mailbox, (msg_t)sentence_pool[pool_idx], TIME_IMMEDIATE);

           pool_idx = (pool_idx + 1) % MAILBOX_SIZE;
         }
         rx_idx = 0;
         frame_started = false;
       }
       else if (frame_started) {
         if (character >= 32 && character <= 126 && rx_idx < (GNSS_BUFFER_SIZE - 1)) {
           rx_buffer[rx_idx++] = character;
         }
       }
    }
  }
}


THD_FUNCTION(thd_gnss_print, arg) {
  (void)arg;
  chRegSetThreadName("GNSS_Processor");

  while (true) {
    msg_t msg;
    msg_t status = chMBFetchTimeout(&rmc_mailbox, &msg, TIME_INFINITE);

    if (status == MSG_OK) {
        char *sentence = (char*)msg;

        if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {

            if (parse_gprmc(sentence, gps_data)) {
                chprintf(chp, "lat: %.6f, long: %.6f\r\n",
                         gps_data->latitude, gps_data->longitude, gps_data->speed_knots, gps_data->heading);
            } else {
                chprintf(chp, "Error: no data.\r\n");
            }
        }
    }
  }
}

void thread_start_gps(gps_data_t* arg) {
  gps_data = (gps_data_t*) arg;
  chThdCreateStatic(waGNSS_Rx, sizeof(waGNSS_Rx), HIGHPRIO, thd_gnss_receive, NULL);
  chThdCreateStatic(waGNSS_Parser, sizeof(waGNSS_Parser), NORMALPRIO-1, thd_gnss_print, NULL);
}
