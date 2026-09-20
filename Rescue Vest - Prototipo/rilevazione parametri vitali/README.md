# Rilevazione parametri vitali

[PPG](PPG/README.md) acquisisce il segnale MAX30102 e calcola BPM.
[MQ2](MQ2/README.md) acquisisce il sensore ambientale gas e controlla il buzzer.
MQ2 è raggruppato qui secondo la struttura richiesta, pur essendo un sensore
ambientale. Entrambi i moduli sono inclusi nella build del client.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | MAX30102 SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x57 | AF4, open-drain, pull-up |
| Client | MQ2 analogico | PA1 | ADC1_IN2 | Ingresso analogico; nessuna AF |
| Client | Buzzer MQ2 | PB4 | TIM3_CH1, PWM 2,5 kHz | AF2 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
