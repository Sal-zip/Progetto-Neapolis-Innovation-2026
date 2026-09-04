/*
    NISC2026 - GestureLogicv3
    DEMO INTEGRAZIONE del modulo gesture di Nello.

    In main restano SOLO le init comuni del progetto e le chiamate ai
    moduli; ogni modulo vive nel suo file/cartella:
      - gesture/ (gesture_init): riconoscimento gesture con VL53L7CX,
        PUBLISHER delle flag su `gesture_events` (vedi gesture_events.h);
      - buzzer/  (buzzer_init):  feedback sonoro per l'operatore,
        buzzer passivo su PB4 (PWM TIM3_CH1);
      - demo/    (demo_init):    codice STAND-IN di demo (LED heartbeat,
        consumer fittizio dei gesti, GPIO demo) da sostituire
        all'integrazione coi moduli veri degli altri membri.

    Comportamento della demo (seriale 38400):
      - al reset -> classico suono di accensione (tre note crescenti,
        automatico in buzzer_init());
      - doppio movimento della mano (2 picchi separati da una pausa)
        -> [!!!] SISTEMA ATTIVATO [!!!] + tre beep corti (sistema da
        SPENTO ad ATTIVO);
      - a sistema ATTIVO, azione laterale sinistra/destra
        -> "<--- AZIONE SINISTRA/DESTRA RILEVATA! (Mano Aperta/Chiusa)"
        + una nota grave (sinistra) / due beep acuti (destra).
      L'operatore riconosce dal suono sia l'accensione sia quale gesto
      e' stato effettivamente riconosciuto.
*/

#include "ch.h"
#include "hal.h"

#include "gesture.h"
#include "buzzer.h"
#include "demo.h"

static BaseSequentialStream *chp = (BaseSequentialStream *)&SD2;

/*
 * Application entry point.
 * In integrazione: qui restano solo le init comuni e le chiamate ai
 * moduli; i thread e i GPIO demo si sostituiscono eliminando il modulo
 * demo/ e chiamando i moduli veri dei membri del team.
 */
int main(void) {

  halInit();
  chSysInit();

  /* Seriale USART2: debug del progetto (SD2, 38400 8N1). */
  palSetPadMode(GPIOA, 2U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3U, PAL_MODE_ALTERNATE(7));
  sdStart(&SD2, NULL);

  /* Modulo gesture (publisher): I2C + VL53L7CX + thread di riconoscimento. */
  gesture_init(chp);

  /* Modulo buzzer (feedback operatore): PWM TIM3_CH1 su PB4. All'avvio
     suona da solo il "rumore di accensione"; per i gesti riconosciuti
     chiamare buzzer_play() (vedi consumer STAND-IN in demo/). */
  buzzer_init();

  /* STAND-IN demo: LED heartbeat, consumer fittizio dei gesti, GPIO demo.
     Sostituire all'integrazione con i moduli veri degli altri membri. */
  demo_init(chp);

  while (true) {
    chThdSleepMilliseconds(500);
  }
}
