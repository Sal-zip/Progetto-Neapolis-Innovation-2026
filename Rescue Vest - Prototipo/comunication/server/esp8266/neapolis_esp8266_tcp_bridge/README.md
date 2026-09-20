# ESP8266 — bridge TCP

Lo sketch mantiene il collegamento Wi-Fi/TCP trasparente; MQTT resta sulla
Nucleo. SSID, password e destinazione TCP arrivano tramite comandi AT dalla
Nucleo. Non contiene una dashboard e non richiede PubSubClient.

Apri `neapolis_esp8266_tcp_bridge.ino` in Arduino IDE con il core ESP8266.
Caricalo sul modem, poi collega PC4 TX della Nucleo server a RX ESP8266 e
PC5 RX a TX ESP8266, con GND comune. UART: 38400 baud, 8-N-1.
Il codice usa `Serial` senza `swap()`: GPIO1 TX e GPIO3 RX su UART0.
Sul client i corrispondenti pin STM32 sono PC10 TX e PC11 RX.

Durante il caricamento evita contese sulla UART scollegando TX/RX della Nucleo.
Alimenta il modulo a 3,3 V con EN/CH_PD alto; i segnali UART sono a 3,3 V.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Server | ESP8266 TX / RX | PC4 / PC5 | USART1, 38400 baud | AF7 |
| Server | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| ESP8266 bridge | UART TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 38400 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
