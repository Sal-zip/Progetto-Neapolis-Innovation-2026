# MQ2 — sensore gas e buzzer

`src/mq2.c` configura ADC1 e acquisisce il canale IN2; `src/thread_mq2.c`
aggiorna lo snapshot e controlla l'allarme acustico. `include/` contiene le
interfacce, `mq2.mk` include i due sorgenti nella build client.

**Il codice finale usa PA1 / ADC1_IN2.** I commenti originali indicano
PA0 / IN1, ma le definizioni sono `MQ2_ADC_PIN = 1U` e `ADC_CHANNEL_IN2`.
I file sono conservati invariati; questa tabella segue le definizioni effettive.

Soglie ADC grezze: inferiore a 150 o superiore a 700. Campionamento ogni
secondo. Buzzer: TIM3, canale ChibiOS 0 (canale hardware 1), clock timer
1 MHz, periodo 400 tick, duty 200 tick: 2,5 kHz al 50% quando attivo.
Il pin ADC è analogico e non ha alternate function.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | MQ2 analogico | PA1 | ADC1_IN2 | Ingresso analogico; nessuna AF |
| Client | Buzzer MQ2 | PB4 | TIM3_CH1, PWM 2,5 kHz | AF2 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
