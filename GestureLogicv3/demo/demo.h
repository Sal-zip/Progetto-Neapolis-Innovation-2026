/*
    NISC2026 - GestureLogicv3
    Modulo: demo STAND-IN (codice non definitivo).

    Qui vive tutto il codice fittizio che prima stava in main.c e che
    all'integrazione verra' sostituito dai moduli veri degli altri membri
    del team:
      - LED verde di demo (heartbeat del programma);
      - consumer fittizio dei gesti riconosciuti dal modulo gesture
        (collega gesture_events -> stampe seriali + buzzer_play());
      - GPIO demo non usati dai moduli gesture/buzzer (LED/attuatori degli
        altri membri): solo configurati a un livello iniziale fisso.
        NB: PB4 NON e' piu' in questo elenco: ora e' il buzzer (modulo
        buzzer/, TIM3_CH1).

    In main.c resta solo la chiamata demo_init().
*/

#ifndef DEMO_H_
#define DEMO_H_

#include "ch.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configura i GPIO demo, il LED di heartbeat e il consumer fittizio dei
 * gesti (stampe seriali + buzzer di conferma). Da chiamare una sola volta,
 * dopo halInit()/chSysInit() e dopo l'init dei moduli gesture e buzzer.
 *
 * dbg: flusso seriale per i messaggi della demo (es. (BaseSequentialStream
 *      *)&SD2), puo' essere NULL.
 */
void demo_init(BaseSequentialStream *dbg);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_H_ */
