/*
    NISC2026 - GestureLogicv3
    Modulo: buzzer di feedback per l'operatore.

    Hardware: buzzer passivo su PA6, canale TIM3_CH1 (PWM).
    Stessa configurazione del buzzer del modulo IR (helmet_buzzer.c).

    Il suono di ACCENSIONE (classico "bip-bip" crescente) viene eseguito
    automaticamente all'avvio: basta chiamare buzzer_init() una volta.

    Gli altri suoni servono da NOTIFY per l'operatore: chi riceve un gesto
    (o una chiamata) da gesture_events chiama buzzer_play() e l'operatore
    riconosce dal suono quale evento e' stato riconosciuto (un bip breve
    con tonalita' diversa per ogni evento):
      - BUZZER_SOUND_ACTIVATION -> doppio gesto: sistema ATTIVATO
                                   (un bip breve di tonalita' media);
      - BUZZER_SOUND_LEFT       -> azione sinistra rilevata
                                   (un bip breve grave);
      - BUZZER_SOUND_RIGHT      -> azione destra rilevata
                                   (un bip breve acuto).

    API NON bloccante: buzzer_play() fa solo una richiesta, il thread
    interno del modulo suona il pattern con le sue temporizzazioni. Se un
    suono e' gia' in corso, la richiesta resta pendente e viene eseguita
    appena libero (eventi troppo ravvicinati dello stesso tipo si fondono,
    ma i gesti del VL53L7CX sono di per se' lenti).
*/

#ifndef BUZZER_H_
#define BUZZER_H_

#include "ch.h"
#include "hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Suoni disponibili (melodie definite in buzzer.c). */
typedef enum {
  BUZZER_SOUND_POWER_ON = 0,   /* Accensione: automatico all'avvio. */
  BUZZER_SOUND_ACTIVATION,     /* Doppio gesto: sistema ATTIVATO. */
  BUZZER_SOUND_LEFT,           /* Azione laterale sinistra. */
  BUZZER_SOUND_RIGHT           /* Azione laterale destra. */
} buzzer_sound_t;

/*
 * Configura il pin PA6 (PWM TIM3_CH1) e avvia il thread interno che
 * esegue i pattern sonori. All'avvio suona BUZZER_SOUND_POWER_ON.
 * Da chiamare una sola volta, dopo halInit()/chSysInit().
 */
void buzzer_init(void);

/*
 * Chiede al modulo di eseguire il suono indicato (non bloccante).
 * Puo' essere chiamata da qualsiasi thread, anche a pattern in corso.
 */
void buzzer_play(buzzer_sound_t sound);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H_ */
