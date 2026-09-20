/*
    NISC2026 - GestureLogicv3
    Modulo: riconoscimento gesture con VL53L7CX (responsabilita' del modulo
            gesture).

    Cosa fa questo modulo:
      - configura le linee I2C del sensore;
      - inizializza il VL53L7CX (ULD + motion indicator 4x4);
      - avvia un thread interno che acquisisce i frame di motion a ~15 Hz;
      - riconosce i gesti con due macchine a stati;
      - PUBBLICA il risultato sull'event source `gesture_events`
        (vedi gesture_events.h). Nessuna logica applicativa qui.

    Cosa NON fa (competenze degli altri):
      - NON decide cosa fare dei gesti riconosciuti: i moduli degli altri
        membri sono SUBSCRIBER che ricevono le flag su `gesture_events` e
        agiscono di conseguenza.

    NOTA SULLA RISOLUZIONE: il motion indicator del VL53L7CX lavora su una
    griglia fissa di 16 aggregati sia in 4x4 che in 8x8 (in 8x8 ogni
    aggregato copre 2x2 zone). Usare 8x8 non aggiunge risoluzione al motion
    e costa piu' banda I2C, quindi si usa il sensore in 4x4.

    Macchine a stati (dettagli in gesture.c):
      - WAKE   (sistema SPENTO): attende il doppio movimento della mano
               (picco -> pausa -> picco -> pausa). Completato -> pubblica
               EVT_GESTURE_DOUBLE_WAVE e porta il sistema ATTIVO.
      - ACTION (sistema ATTIVO): attende un picco di motion dominante su una
               zona laterale seguito dal ritorno della mano. Completato ->
               pubblica EVT_GESTURE_LEFT o EVT_GESTURE_RIGHT.
*/

#ifndef GESTURE_H_
#define GESTURE_H_

#include "ch.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configura l'I2C del sensore, inizializza il VL53L7CX e avvia il thread
 * interno di acquisizione/riconoscimento (che pubblica su `gesture_events`).
 * Da chiamare una sola volta, dopo halInit()/chSysInit().
 *
 * dbg: flusso seriale per i messaggi di avanzamento del riconoscimento
 *      (es. (BaseSequentialStream *)&SD2), puo' essere NULL.
 */
void gesture_init(BaseSequentialStream *dbg);

#ifdef __cplusplus
}
#endif

#endif /* GESTURE_H_ */
