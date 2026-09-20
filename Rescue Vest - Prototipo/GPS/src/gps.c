#include "gps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Risorse condivise con thread_gps.c.
 * La mailbox trasporta gli indici delle frasi NMEA disponibili.
 */
msg_t rmc_mailbox_buffer[MAILBOX_SIZE];
mailbox_t rmc_mailbox;

/* Pool statico usato per memorizzare le frasi ricevute dal GPS. */
char sentence_pool[MAILBOX_SIZE][GNSS_BUFFER_SIZE];

/*
 * Indice usato dalla versione originale del thread GPS.
 * Con la gestione free/ready del nuovo thread_gps.c non è più necessario,
 * ma può rimanere temporaneamente per compatibilità.
 */
uint8_t pool_idx = 0U;

/* Configurazione di USART1 per il modulo Teseo-VIC3DA. */
static const SerialConfig gnsssd1cfg = {
  115200U, /* Baud rate. */
  0U,      /* Registro CR1: configurazione standard. */
  0U,      /* Registro CR2: un bit di stop. */
  0U       /* Registro CR3: nessun controllo di flusso. */
};

/**
 * Invia un comando al modulo GNSS attraverso USART1.
 */
void sendgnss(const char *msg) {
  sdWrite(&SD1, (const uint8_t *)msg, strlen(msg));

  /*
   * Lascia al modulo il tempo di elaborare il comando
   * prima dell'eventuale comando successivo.
   */
  chThdSleepMilliseconds(50);
}

/**
 * Converte una coordinata NMEA dal formato:
 *
 *   gradi-minuti: DDMM.MMMM
 *
 * al formato decimale:
 *
 *   DD.DDDDDD
 */
double convert_nmea_to_decimal(double raw_nmea) {
  const int degrees = (int)(raw_nmea / 100.0);
  const double minutes = raw_nmea - ((double)degrees * 100.0);

  return (double)degrees + (minutes / 60.0);
}

/**
 * Converte manualmente una stringa numerica in float.
 *
 * Evita di dipendere direttamente da strtof() e gestisce
 * anche valori negativi e cifre decimali.
 */
float parse_float_custom(const char *s) {
  float value = 0.0f;
  float factor = 1.0f;
  int decimal_part = 0;

  if ((s == NULL) || (*s == '\0')) {
    return 0.0f;
  }

  if (*s == '-') {
    ++s;
    factor = -1.0f;
  }

  while (*s != '\0') {
    if (*s == '.') {
      decimal_part = 1;
      ++s;
      continue;
    }

    if (decimal_part != 0) {
      factor /= 10.0f;
    }

    if ((*s >= '0') && (*s <= '9')) {
      value = (value * 10.0f) + (float)(*s - '0');
    }

    ++s;
  }

  return value * factor;
}

/**
 * Estrae i dati principali da una frase NMEA RMC.
 *
 * Una frase RMC contiene:
 * - validità della posizione;
 * - latitudine e longitudine;
 * - velocità rispetto al suolo;
 * - direzione di movimento.
 */
bool parse_gprmc(const char *nmea_str, gps_data_t *rmc_out) {
  /*
   * Cerca "RMC" senza dipendere dal talker ID.
   * Sono quindi accettate, per esempio, $GPRMC e $GNRMC.
   */
  const char *cursor = strstr(nmea_str, "RMC");

  if (cursor == NULL) {
    return false;
  }

  /* Posiziona il cursore sulla virgola successiva a "RMC". */
  cursor += 3;

  float raw_latitude = 0.0f;
  float raw_longitude = 0.0f;
  float speed = 0.0f;
  float heading = 0.0f;

  /* Valori predefiniti corrispondenti a una posizione non valida. */
  char status = 'V';
  char north_south = 'N';
  char east_west = 'E';

  int field_index = 0;

  /*
   * Scorre i campi separati da virgole fino al carattere
   * che introduce il checksum NMEA.
   */
  while ((*cursor != '\0') && (*cursor != '*')) {
    if (*cursor == ',') {
      ++field_index;
      ++cursor;
      continue;
    }

    char field[16];
    size_t index = 0U;

    /* Copia il campo corrente in un buffer locale. */
    while ((*cursor != '\0') &&
           (*cursor != ',') &&
           (*cursor != '*') &&
           (index < (sizeof(field) - 1U))) {
      field[index++] = *cursor++;
    }

    field[index] = '\0';

    /*
     * Campi RMC utilizzati:
     *
     * 2 = stato A/V
     * 3 = latitudine
     * 4 = emisfero N/S
     * 5 = longitudine
     * 6 = emisfero E/W
     * 7 = velocità in nodi
     * 8 = direzione in gradi
     */
    switch (field_index) {
      case 2:
        status = field[0];
        break;

      case 3:
        raw_latitude = parse_float_custom(field);
        break;

      case 4:
        north_south = field[0];
        break;

      case 5:
        raw_longitude = parse_float_custom(field);
        break;

      case 6:
        east_west = field[0];
        break;

      case 7:
        speed = parse_float_custom(field);
        break;

      case 8:
        heading = parse_float_custom(field);
        break;

      default:
        /* Gli altri campi RMC non vengono utilizzati. */
        break;
    }
  }

  /*
   * Lo stato 'A' indica una posizione valida.
   * Lo stato 'V' indica invece che il fix non è disponibile.
   */
  if ((status != 'A') ||
      (raw_latitude == 0.0f) ||
      (raw_longitude == 0.0f)) {
    rmc_out->is_valid = false;
    return false;
  }

  /* Converte la latitudine nel formato decimale. */
  rmc_out->latitude =
      convert_nmea_to_decimal((double)raw_latitude);

  if (north_south == 'S') {
    rmc_out->latitude = -rmc_out->latitude;
  }

  /* Converte la longitudine nel formato decimale. */
  rmc_out->longitude =
      convert_nmea_to_decimal((double)raw_longitude);

  if (east_west == 'W') {
    rmc_out->longitude = -rmc_out->longitude;
  }

  rmc_out->speed_knots = speed;
  rmc_out->heading = heading;
  rmc_out->is_valid = true;

  return true;
}

/**
 * Inizializza il collegamento con il modulo GNSS.
 *
 * Il GPS utilizza:
 * - PA9 come USART1_TX;
 * - PA10 come USART1_RX;
 * - SD1 a 115200 baud.
 */
void init_gnss(void) {
  palSetLineMode(
      LINE_GNSS_TX,
      PAL_MODE_ALTERNATE(7) | PAL_MODE_OUTPUT_PUSHPULL);

  palSetLineMode(
      LINE_GNSS_RX,
      PAL_MODE_INPUT_PULLUP | PAL_MODE_ALTERNATE(7));

  /* Avvia USART1 con la configurazione del Teseo-VIC3DA. */
  sdStart(&SD1, &gnsssd1cfg);

  /* Inizializza la mailbox delle frasi NMEA pronte. */
  chMBObjectInit(
      &rmc_mailbox,
      rmc_mailbox_buffer,
      MAILBOX_SIZE);

  /*
   * Configurazione persistente del modulo GNSS.
   *
   * ATTENZIONE: questi comandi provengono dalla versione originale,
   * ma parametri e checksum devono essere verificati prima della build
   * definitiva. In particolare, listid 3 seleziona l'uscita I2C e non
   * quella UART utilizzata da questo driver.
   */
  sendgnss("$PSTMCFGSETFIXRATE,1,1000*5F\r\n");
  sendgnss("$PSTMCFGMSGL,3,1,1,0*1D\r\n");
  sendgnss("$PSTMSAVEPAR*5C\r\n");
}
