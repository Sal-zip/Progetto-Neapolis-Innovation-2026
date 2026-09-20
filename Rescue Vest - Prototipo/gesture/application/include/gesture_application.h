#ifndef GESTURE_APPLICATION_H
#define GESTURE_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Avvia la logica applicativa della Gesture Board.
 *
 * Inizializza:
 *   - seriale di debug USART2;
 *   - buzzer locale;
 *   - collegamento verso la board principale;
 *   - sensore VL53L7CX e riconoscimento gesture.
 *
 * La funzione deve essere chiamata una sola volta, dopo:
 *
 *   halInit();
 *   chSysInit();
 */
void gesture_application_start(void);

#ifdef __cplusplus
}
#endif

#endif /* GESTURE_APPLICATION_H */
