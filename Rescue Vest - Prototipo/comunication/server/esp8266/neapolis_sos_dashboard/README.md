# ESP8266 — dashboard SOS

Lo sketch si collega a Wi-Fi e MQTT, sottoscrive i tre topic e serve la
pagina HTTP. Conserva gli ultimi venti messaggi in RAM; il riavvio perde la
cronologia. Coordinate E7, BPM, ADC MQ2, SOS e stato sono visualizzati dal
browser. Lo stato online è stimato dalla ricezione recente di dati.

Usa un ESP8266 separato dal modem della Nucleo. Apri lo sketch omonimo in
Arduino IDE, installa il core ESP8266 e PubSubClient e configura rete e
broker nelle definizioni iniziali dello sketch. Mantieni un Client ID distinto. Il monitor seriale è a 115200 baud e
mostra l'IP da aprire nel browser. Leaflet e le tessere OpenStreetMap
richiedono accesso Internet dal browser.

La dashboard non configura sensori o altre linee GPIO. Usa `Serial` sulla
UART0 predefinita; non usa alternate function numerate STM32.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| ESP8266 dashboard | Console TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 115200 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.
