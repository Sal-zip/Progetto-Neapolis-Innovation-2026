#ifndef GESTURE_LINK_H
#define GESTURE_LINK_H

#include <stdbool.h>

#include "gesture_protocol.h"

typedef void (*gesture_link_callback_t)(gesture_command_t command);

/*
 * Inizializzazione lato board gesture.
 */
bool gesture_link_tx_start(void);

/*
 * Invia un comando alla board principale.
 *
 * Restituisce true soltanto se viene ricevuto l'ACK.
 */
bool gesture_link_send(gesture_command_t command);

/*
 * Inizializzazione lato board principale.
 *
 * La callback viene eseguita dal thread ricevitore e dovrebbe quindi
 * limitarsi a inserire l'evento nella mailbox/coda applicativa.
 */
bool gesture_link_start(gesture_link_callback_t callback);

#endif
