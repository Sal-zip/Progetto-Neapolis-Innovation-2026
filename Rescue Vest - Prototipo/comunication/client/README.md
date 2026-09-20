# Client STM32

Firmware principale `sos_client`. `application/src/main.c` è l'unico entry
point; `application.c` avvia sensori, ricevitore gesture e pubblicazione MQTT.
`src/` contiene il trasporto ESP8266 e il client MQTT; `include/` contiene le
interfacce e la configurazione. `config/` contiene kernel, HAL e MCU.

I moduli GPS, PPG e MQ2 si trovano nelle rispettive cartelle del prototipo.
Il ricevitore GestureLink si trova in `gesture/link`; il ruolo del client è `RX`.
Le versioni obsolete `sos_client.c`, `sos_outbox.c`, `sos_request.c` e il
vecchio `Communication/main.c` non facevano parte della build finale.

La configurazione si trova in `include/app_config.h`. L'ESP8266 usa USART3; USART1 è riservata al GPS.
Per GestureLink il codice chiama `sioStart(..., NULL)`: usa il default SIO
in `config/halconf.h` (38400 baud nei file forniti).

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
