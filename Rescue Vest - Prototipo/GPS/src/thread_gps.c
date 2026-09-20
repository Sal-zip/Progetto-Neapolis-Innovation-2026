#include "thread_gps.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * rmc_mailbox, rmc_mailbox_buffer e sentence_pool sono definiti
 * in gps.c e inizializzati da init_gnss().
 *
 * rmc_mailbox contiene gli indici degli slot pronti per il parsing.
 * free_mailbox contiene invece gli indici degli slot disponibili.
 */
static mailbox_t free_mailbox;
static msg_t free_mailbox_buffer[MAILBOX_SIZE];

/* Ultima posizione GPS valida, accessibile attraverso gps_get_latest(). */
static gps_data_t latest_gps_data;
static mutex_t latest_gps_mutex;
static systime_t latest_gps_update;

/* Indica se è già stata ricevuta almeno una posizione valida. */
static bool latest_gps_available;

/* Impedisce l'avvio multiplo dei thread GPS. */
static bool gps_service_started;

/* Memoria statica dei thread ChibiOS. */
static THD_WORKING_AREA(wa_gps_receiver, 512);
static THD_WORKING_AREA(wa_gps_parser, 1024);

/**
 * Verifica se la frase ricevuta è una frase NMEA RMC.
 *
 * Sono accettati tutti i talker ID:
 * $GPRMC, $GNRMC, $GARMC e così via.
 */
static bool is_rmc_sentence(const char *sentence, size_t length) {
  if ((sentence == NULL) || (length < 6U)) {
    return false;
  }

  return (sentence[0] == '$') &&
         (sentence[3] == 'R') &&
         (sentence[4] == 'M') &&
         (sentence[5] == 'C');
}

/**
 * Copia una frase NMEA in uno slot libero e ne invia l'indice
 * al thread di parsing.
 *
 * Se tutti gli slot sono occupati, la frase viene scartata. È preferibile
 * perdere una singola frase GPS piuttosto che bloccare il thread che riceve
 * i caratteri dalla UART.
 */
static void queue_rmc_sentence(const char *sentence) {
  msg_t slot_message;

  if (sentence == NULL) {
    return;
  }

  if (chMBFetchTimeout(&free_mailbox,
                       &slot_message,
                       TIME_IMMEDIATE) != MSG_OK) {
    return;
  }

  const size_t slot = (size_t)slot_message;

  if (slot >= MAILBOX_SIZE) {
    return;
  }

  strncpy(sentence_pool[slot], sentence, GNSS_BUFFER_SIZE - 1U);
  sentence_pool[slot][GNSS_BUFFER_SIZE - 1U] = '\0';

  if (chMBPostTimeout(&rmc_mailbox,
                      (msg_t)slot,
                      TIME_IMMEDIATE) != MSG_OK) {
    /*
     * La mailbox delle frasi pronte è piena: restituisce immediatamente
     * lo slot alla mailbox degli slot liberi.
     */
    (void)chMBPostTimeout(&free_mailbox,
                          (msg_t)slot,
                          TIME_IMMEDIATE);
  }
}

/**
 * Thread di ricezione.
 *
 * Rimane bloccato su SD1 fino all'arrivo di un carattere. Ricostruisce
 * le frasi NMEA delimitate da '$' e CR/LF e accoda soltanto quelle RMC.
 */
static THD_FUNCTION(gps_receiver_thread, argument) {
  (void)argument;

  chRegSetThreadName("gps-receiver");

  char receive_buffer[GNSS_BUFFER_SIZE];
  size_t receive_index = 0U;
  bool frame_started = false;

  while (true) {
    const msg_t received = sdGetTimeout(&SD1, TIME_INFINITE);

    if ((received == MSG_RESET) || (received == MSG_TIMEOUT)) {
      continue;
    }

    const char character = (char)received;

    /*
     * L'arrivo di '$' avvia una nuova frase e permette anche
     * di risincronizzarsi dopo un eventuale frame incompleto.
     */
    if (character == '$') {
      receive_index = 0U;
      receive_buffer[receive_index++] = character;
      frame_started = true;
      continue;
    }

    if (!frame_started) {
      continue;
    }

    if ((character == '\r') || (character == '\n')) {
      receive_buffer[receive_index] = '\0';

      if (is_rmc_sentence(receive_buffer, receive_index)) {
        queue_rmc_sentence(receive_buffer);
      }

      receive_index = 0U;
      frame_started = false;
      continue;
    }

    /*
     * Le frasi NMEA sono ASCII stampabile.
     * Se il frame supera il buffer viene scartato interamente.
     */
    if ((character < 32) || (character > 126)) {
      continue;
    }

    if (receive_index >= (GNSS_BUFFER_SIZE - 1U)) {
      receive_index = 0U;
      frame_started = false;
      continue;
    }

    receive_buffer[receive_index++] = character;
  }
}

/**
 * Thread di parsing.
 *
 * Preleva gli indici degli slot pronti, interpreta la frase RMC
 * e aggiorna in mutua esclusione l'ultima posizione valida.
 */
static THD_FUNCTION(gps_parser_thread, argument) {
  (void)argument;

  chRegSetThreadName("gps-parser");

  while (true) {
    msg_t slot_message;

    if (chMBFetchTimeout(&rmc_mailbox,
                         &slot_message,
                         TIME_INFINITE) != MSG_OK) {
      continue;
    }

    const size_t slot = (size_t)slot_message;

    if (slot < MAILBOX_SIZE) {
      gps_data_t parsed_data = {0};

      if (parse_gprmc(sentence_pool[slot], &parsed_data)) {
        chMtxLock(&latest_gps_mutex);

        latest_gps_data = parsed_data;
        latest_gps_update = chVTGetSystemTimeX();
        latest_gps_available = true;

        chMtxUnlock(&latest_gps_mutex);
      }

      /*
       * Il parser ha terminato di usare la stringa: lo slot può
       * essere restituito al thread di ricezione.
       */
      (void)chMBPostTimeout(&free_mailbox,
                            (msg_t)slot,
                            TIME_INFINITE);
    }
  }
}

void gps_start(void) {
  if (gps_service_started) {
    return;
  }

  memset(&latest_gps_data, 0, sizeof(latest_gps_data));

  latest_gps_update = 0U;
  latest_gps_available = false;

  chMtxObjectInit(&latest_gps_mutex);

  /*
   * init_gnss() configura SD1, PA9/PA10 e inizializza rmc_mailbox.
   */
  init_gnss();

  /*
   * Inizializza la mailbox degli slot liberi e vi inserisce
   * inizialmente tutti gli indici del pool.
   */
  chMBObjectInit(&free_mailbox,
                 free_mailbox_buffer,
                 MAILBOX_SIZE);

  for (size_t slot = 0U; slot < MAILBOX_SIZE; ++slot) {
    (void)chMBPostTimeout(&free_mailbox,
                          (msg_t)slot,
                          TIME_IMMEDIATE);
  }

  gps_service_started = true;

  (void)chThdCreateStatic(
      wa_gps_receiver,
      sizeof(wa_gps_receiver),
      HIGHPRIO,
      gps_receiver_thread,
      NULL);

  (void)chThdCreateStatic(
      wa_gps_parser,
      sizeof(wa_gps_parser),
      NORMALPRIO,
      gps_parser_thread,
      NULL);
}

bool gps_get_latest(gps_data_t *out, sysinterval_t max_age) {
  if ((out == NULL) || !gps_service_started) {
    return false;
  }

  bool result = false;

  chMtxLock(&latest_gps_mutex);

  if (latest_gps_available) {
    const sysinterval_t age =
        chVTTimeElapsedSinceX(latest_gps_update);

    if ((max_age == TIME_INFINITE) || (age <= max_age)) {
      *out = latest_gps_data;
      result = true;
    }
  }

  chMtxUnlock(&latest_gps_mutex);

  return result;
}
