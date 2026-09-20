# rilevazione parametri vitali/PPG/include

Header e interfacce del modulo; questi file non inizializzano da soli le periferiche.

Consulta il [README del modulo](../README.md) per descrizione e collegamenti.

File: `ppg.h`, `thread_ppg.h`.

La tabella riporta il contesto hardware del modulo di appartenenza;
non implica che ogni file di questa cartella configuri tutti i pin elencati.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | MAX30102 SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x57 | AF4, open-drain, pull-up |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
