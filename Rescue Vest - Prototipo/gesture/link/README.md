# GestureLink — protocollo condiviso

`gesture_protocol.c` codifica e decodifica frame di 8 byte con comando,
sequenza e controllo di integrità. `gesture_link_tx.c` invia i comandi e
attende ACK; `gesture_link_rx.c` riceve, filtra duplicati e invia ACK.
Il file `.mk` seleziona uno dei due ruoli per ciascuna build.

Il link usa LPUART1 tramite `LPSIOD1`/SIO e il default 38400 baud di `halconf.h`.
Incrocia TX/RX fra le due schede e collega le masse. Il modulo protocollo puro
non configura pin; i pin vengono impostati dalle implementazioni TX/RX.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client e gesture | GestureLink TX / RX | PC1 / PC0 | LPUART1 / SIO | AF8 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
