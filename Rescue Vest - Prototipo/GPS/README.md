# GPS — Teseo-VIC3DA

`src/gps.c` configura USART1, invia i comandi GNSS e interpreta le frasi RMC.
`src/thread_gps.c` separa ricezione e parsing con mailbox e pubblica lo snapshot.
`include/` contiene tipi e interfacce; `gps.mk` integra il modulo nella build
client. La seriale è a 115200 baud. Il TX della MCU PA9 va al RX del GPS,
mentre il TX del GPS va a PA10.

L'inizializzazione conserva i comandi di configurazione GNSS della versione
fornita, inclusi quelli persistenti. Nessun pin PPS o reset è configurato qui.
Il modulo si compila insieme al client, non come firmware autonomo.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | GPS TX / RX | PA9 / PA10 | USART1, 115200 baud | AF7 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
