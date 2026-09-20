# comunication/client/src

Sorgenti C del modulo.

Consulta il [README del modulo](../README.md) per descrizione e collegamenti.

File: `esp8266_transport.c`, `mqtt_client.c`.

La tabella riporta il contesto hardware del modulo di appartenenza;
non implica che ogni file di questa cartella configuri tutti i pin elencati.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | ESP8266 TX / RX | PC10 / PC11 | USART3, 38400 baud | AF7 |
| Client | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| Client | GestureLink TX / RX | PC1 / PC0 | LPUART1, driver SIO | AF8 |
| Client | GPS TX / RX | PA9 / PA10 | USART1, 115200 baud | AF7 |
| Client | MAX30102 SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x57 | AF4, open-drain, pull-up |
| Client | MQ2 analogico | PA1 | ADC1_IN2 | Ingresso analogico; nessuna AF |
| Client | Buzzer MQ2 | PB4 | TIM3_CH1, PWM 2,5 kHz | AF2 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
