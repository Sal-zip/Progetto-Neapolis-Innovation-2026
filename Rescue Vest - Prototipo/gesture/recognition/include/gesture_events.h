/*
    NISC2026 - GestureLogicv3
    Modulo: contratto condiviso per il riconoscimento gesture.

    Questo header e' l'INTERFACCIA tra il modulo gesture (publisher) e il
    resto del team, quindi e' l'unico file del modulo che va condiviso
    (stesso pattern del pulsante, vedi helmet_events.h):

      - PUBLISHER (gesture.c):  il thread interno acquisisce il VL53L7CX
                                (motion indicator 4x4), riconosce il gesto
                                e fa BROADCAST delle flag qui sotto
                                sull'event source `gesture_events`.
      - SUBSCRIBER (altri moduli / thread operatore):  si registrano su
                                `gesture_events` con chEvtRegisterMask() e
                                leggono le flag con chEvtGetAndClearFlags().

    Nessuno ha bisogno del codice dell'altro: basta includere questo header
    e linkare gesture.c quando verra' fatta l'integrazione.

    Gesti pubblicati (dettaglio delle FSM in gesture.c):
      - EVT_GESTURE_DOUBLE_WAVE: doppio movimento della mano (due picchi di
                                 motion separati da una pausa) a sistema
                                 SPENTO -> il sistema si ATTIVA;
      - EVT_GESTURE_LEFT:        a sistema ATTIVO, picco di motion sulla
                                 zona SINISTRA seguito dal ritorno della
                                 mano;
      - EVT_GESTURE_RIGHT:       come EVT_GESTURE_LEFT ma zona DESTRA.
*/

#ifndef GESTURE_EVENTS_H_
#define GESTURE_EVENTS_H_

#include "ch.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Event source del riconoscimento gesture. Definito in gesture.c. */
extern event_source_t gesture_events;

/* Doppio movimento completato: sistema spento -> attivo. */
#define EVT_GESTURE_DOUBLE_WAVE ((eventflags_t)(1U << 0))

/* Azione laterale completata (mano rientrata dopo il picco). */
#define EVT_GESTURE_LEFT        ((eventflags_t)(1U << 1))
#define EVT_GESTURE_RIGHT       ((eventflags_t)(1U << 2))

#ifdef __cplusplus
}
#endif

#endif /* GESTURE_EVENTS_H_ */
