#ifndef APPLICATION_H
#define APPLICATION_H

#include "gesture_protocol.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inizializza comunicazione, sensori e thread applicativi.
 * Deve essere chiamata una sola volta dopo halInit() e chSysInit().
 */
void application_start(void);

/*
 * Callback invocata dal ricevitore collegato alla board Gesture.
 * Non esegue direttamente operazioni MQTT bloccanti.
 */
void application_on_gesture(gesture_command_t command);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_H */
