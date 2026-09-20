# comunication/server/src

Sorgenti C del modulo.

Consulta il [README del modulo](../README.md) per descrizione e collegamenti.

File: `esp8266_transport.c`, `mqtt_client.c`, `mqtt_inbox.c`.

La tabella riporta il contesto hardware del modulo di appartenenza;
non implica che ogni file di questa cartella configuri tutti i pin elencati.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Server | ESP8266 TX / RX | PC4 / PC5 | USART1, 38400 baud | AF7 |
| Server | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
