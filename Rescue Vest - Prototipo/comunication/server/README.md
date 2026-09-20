# Server STM32 ed ESP8266

Firmware `sos_server`: `main.c` avvia USART1 verso il modem e USART2 per debug;
`src/` implementa trasporto TCP ESP8266, MQTT e inbox. Si sottoscrive ai tre
topic SOS, telemetria e stato. Il broker MQTT deve essere disponibile in rete.

La configurazione originale è in `include/app_config.h`.

`esp8266/` contiene due programmi indipendenti: il bridge TCP per il modem
della Nucleo e la dashboard per un secondo ESP8266. Non si caricano entrambi
sullo stesso modulo. Client e server hanno pin diversi verso il modem.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Server | ESP8266 TX / RX | PC4 / PC5 | USART1, 38400 baud | AF7 |
| Server | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| ESP8266 bridge | UART TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 38400 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |
| ESP8266 dashboard | Console TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 115200 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
