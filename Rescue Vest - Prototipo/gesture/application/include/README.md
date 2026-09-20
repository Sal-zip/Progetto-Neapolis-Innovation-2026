# gesture/application/include

Header e interfacce del modulo; questi file non inizializzano da soli le periferiche.

Consulta il [README del modulo](../../README.md) per descrizione e collegamenti.

File: `gesture_application.h`.

La tabella riporta il contesto hardware del modulo di appartenenza;
non implica che ogni file di questa cartella configuri tutti i pin elencati.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Gesture | VL53L7CX SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x29 | AF4, open-drain, pull-up |
| Gesture | GestureLink TX / RX | PC1 / PC0 | LPUART1, driver SIO | AF8 |
| Gesture | Buzzer | PB4 | TIM3_CH1, PWM | AF2 |
| Gesture | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
