# PPG — MAX30102

`src/ppg.c` configura I2C1 e i registri del MAX30102; `src/thread_ppg.c`
elabora i campioni, aggiorna lo snapshot e rileva le condizioni di allarme.
`include/` espone driver e servizio; `ppg.mk` aggiunge entrambi alla build.

Indirizzo I2C a 7 bit: `0x57`. Il timing `0x20303E5D` è descritto nei sorgenti
originali come 100 kHz con PCLK1 a 85 MHz. I pin passano temporaneamente a GPIO
open-drain durante il recupero del bus e tornano in AF4. Il modulo non configura
un pin interrupt. Le soglie BPM nel thread sono 40 e 120.

Il modulo fa parte del firmware client.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | MAX30102 SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x57 | AF4, open-drain, pull-up |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
