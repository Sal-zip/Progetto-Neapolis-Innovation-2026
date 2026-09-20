# gesture/vl53l7cx/inc

Header pubblici del driver ST, incluso il buffer firmware necessario al sensore.

Consulta il [README del modulo](../../README.md) per descrizione e collegamenti.

File: `vl53l7cx_api.h`, `vl53l7cx_buffers.h`, `vl53l7cx_plugin_detection_thresholds.h`, `vl53l7cx_plugin_motion_indicator.h`, `vl53l7cx_plugin_xtalk.h`.

La tabella riporta il contesto hardware del modulo di appartenenza;
non implica che ogni file di questa cartella configuri tutti i pin elencati.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Gesture | VL53L7CX SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x29 | AF4, open-drain, pull-up |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
