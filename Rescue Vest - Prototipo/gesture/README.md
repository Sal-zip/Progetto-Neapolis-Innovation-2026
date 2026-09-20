# Gesture — firmware della scheda dedicata

Firmware `gesture_board`, con `main.c` come entry point e `config/` dedicata.
`recognition/` elabora i frame del VL53L7CX; `vl53l7cx/` contiene il driver ST,
i plugin e il porting ChibiOS. `application/` traduce gli eventi in comandi,
`buzzer/` produce il feedback acustico e `link/` gestisce protocollo e ACK.

Il Makefile originale è conservato nella cartella del modulo.

Il Makefile seleziona GestureLink `TX`, mentre il client seleziona `RX`.
Entrambi i lati trasmettono: la scheda gesture invia comandi e il client
restituisce ACK. Collega PC1 della scheda gesture a PC0 del client e PC0
della scheda gesture a PC1 del client, con massa comune. `sioStart(..., NULL)`
usa il default SIO di `halconf.h`, pari a 38400 baud nei file forniti.

VL53L7CX usa indirizzo a 7 bit `0x29`, acquisizione a 15 Hz e polling a 20 ms.
I sorgenti non configurano pin dedicati di reset o interrupt del sensore.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Gesture | VL53L7CX SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x29 | AF4, open-drain, pull-up |
| Gesture | GestureLink TX / RX | PC1 / PC0 | LPUART1, driver SIO | AF8 |
| Gesture | Buzzer | PB4 | TIM3_CH1, PWM | AF2 |
| Gesture | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
