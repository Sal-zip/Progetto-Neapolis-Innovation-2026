# Firmware ESP8266

Due sketch per due dispositivi diversi:

- `neapolis_esp8266_tcp_bridge`: modem Wi-Fi/TCP comandato dalla Nucleo.
- `neapolis_sos_dashboard`: dashboard HTTP e sottoscrizione MQTT autonoma.

Apri lo sketch desiderato con Arduino IDE e seleziona la scheda ESP8266 reale.
Il core deve fornire ESP8266WiFi e
ESP8266WebServer; la dashboard richiede anche PubSubClient.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| ESP8266 bridge | UART TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 38400 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |
| ESP8266 dashboard | Console TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 115200 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
