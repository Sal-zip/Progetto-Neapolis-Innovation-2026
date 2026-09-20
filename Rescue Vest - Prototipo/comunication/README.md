# Comunicazione

[Client](client/README.md): pubblicazione MQTT di telemetria, stato e allarmi.
[Server](server/README.md): sottoscrizione MQTT e visualizzazione con ESP8266.
Il nome `comunication` è quello concordato per la repository.

I trasporti ESP8266 client/server sono mantenuti separati perché le versioni
fornite differiscono. Il link tra schede gesture e client si trova in
[`gesture/link`](../gesture/link/README.md), con un solo protocollo condiviso.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | ESP8266 TX / RX | PC10 / PC11 | USART3, 38400 baud | AF7 |
| Client | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| Client | GestureLink TX / RX | PC1 / PC0 | LPUART1, driver SIO | AF8 |
| Server | ESP8266 TX / RX | PC4 / PC5 | USART1, 38400 baud | AF7 |
| Server | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
