# Rescue Vest - Prototipo

Progetto Rescue Vest per Neapolis Innovation 2026, organizzato per modulo.

```text
Rescue Vest - Prototipo/
├── comunication/
│   ├── client/       # Applicazione principale e comunicazione MQTT
│   └── server/       # Server STM32 e sketch ESP8266
├── rilevazione parametri vitali/
│   ├── PPG/          # Sensore MAX30102 e frequenza cardiaca
│   └── MQ2/          # Sensore gas e buzzer
├── GPS/              # Teseo-VIC3DA e parsing NMEA
└── gesture/          # Scheda VL53L7CX e collegamento con il client
```

## Moduli

- [Comunicazione](comunication/README.md): client e server MQTT.
- [PPG e MQ2](rilevazione%20parametri%20vitali/README.md): acquisizione sensori.
- [GPS](GPS/README.md): ricezione e interpretazione delle coordinate.
- [Gesture](gesture/README.md): riconoscimento gesti e invio al client.

Ogni cartella contiene un README con descrizione, pinout e alternate function.
I pin sono ricavati dalle definizioni e dalle inizializzazioni dei sorgenti.

Il sistema comprende tre firmware STM32 distinti: client, server e scheda
gesture. PPG, MQ2 e GPS appartengono al client. Il protocollo GestureLink è
condiviso fra client e scheda gesture. Due sketch separati gestiscono il
bridge TCP ESP8266 e la dashboard su un secondo ESP8266. Il broker MQTT è
un servizio esterno.

## Contenuto dell'archivio

I sorgenti, le configurazioni e i Makefile provengono dalla versione finale
fornita. Questa è una riorganizzazione documentale: i Makefile conservano i
percorsi originali e non sono stati adattati alla nuova disposizione.
Non sono state eseguite compilazioni del firmware né prove sulle schede.

Sono conservati anche i driver ST VL53L7CX, i plugin, il buffer firmware
necessario al sensore e gli avvisi di copyright/licenza presenti nei file.
Sono esclusi risultati di compilazione, `.dep/`, backup `.bak`, `.DS_Store`,
metadati Eclipse, launch/debug e script con percorsi locali. Sono omessi i
vecchi sorgenti client `Communication/main.c`, `sos_client`, `sos_outbox` e
`sos_request`, già esclusi dal Makefile finale fornito.

La vecchia cartella `RT-STM32G474RE-BPM-MQ2_complete` è sostituita dalla
versione finale e resta disponibile nella cronologia Git.

## Credenziali di esempio

SSID e password Wi-Fi sono sostituiti da un facsimile nei file
`comunication/client/include/app_config.h`,
`comunication/server/include/app_config.h` e nello sketch
`comunication/server/esp8266/neapolis_sos_dashboard/neapolis_sos_dashboard.ino`:

```c
#define APP_WIFI_SSID     "RESCUE_VEST_WIFI_ESEMPIO"
#define APP_WIFI_PASSWORD "PASSWORD_WIFI_DI_ESEMPIO"
```

Sono valori dimostrativi da sostituire localmente con quelli della propria
rete. Il resto dei sorgenti originali è invariato.

## Contratto MQTT

| Topic | Contenuto |
| --- | --- |
| `neapolis/2026/sos/requests` | SOS e allarmi PPG/MQ2 |
| `neapolis/2026/operators/telemetry` | Telemetria GPS, PPG e MQ2 |
| `neapolis/2026/operators/status` | Attivazione e zona sicura |

I payload usano `schemaVersion: 1`. Le coordinate sono interi E7; MQ2 è un
valore ADC grezzo. Le configurazioni originali indicano indirizzi del broker
diversi tra client e server: occorre allinearli alla rete effettivamente usata.

## Pinout e alternate function

| Scheda | Segnale | Pin MCU | Periferica | Alternate function / modalità |
| --- | --- | --- | --- | --- |
| Client | ESP8266 TX / RX | PC10 / PC11 | USART3, 38400 baud | AF7 |
| Client | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| Client | GestureLink TX / RX | PC1 / PC0 | LPUART1, driver SIO | AF8 |
| Client | GPS TX / RX | PA9 / PA10 | USART1, 115200 baud | AF7 |
| Client | MAX30102 SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x57 | AF4, open-drain, pull-up |
| Client | MQ2 analogico | PA1 | ADC1_IN2 | Ingresso analogico; nessuna AF |
| Client | Buzzer MQ2 | PB4 | TIM3_CH1, PWM 2,5 kHz | AF2 |
| Server | ESP8266 TX / RX | PC4 / PC5 | USART1, 38400 baud | AF7 |
| Server | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| Gesture | VL53L7CX SCL / SDA | PB8 / PB9 | I2C1, indirizzo 0x29 | AF4, open-drain, pull-up |
| Gesture | GestureLink TX / RX | PC1 / PC0 | LPUART1, driver SIO | AF8 |
| Gesture | Buzzer | PB4 | TIM3_CH1, PWM | AF2 |
| Gesture | Console TX / RX | PA2 / PA3 | USART2, 38400 baud | AF7 |
| ESP8266 bridge | UART TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 38400 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |
| ESP8266 dashboard | Console TX / RX | GPIO1 / GPIO3 (UART0 predefinita) | Serial, 115200 baud | Mux gestito dal core ESP8266; nessuna AF STM32 |

I pin sono indicati dal punto di vista della MCU: TX va a RX del dispositivo
collegato e viceversa, con massa comune. Le AF STM32 sono quelle impostate nei
sorgenti. Le schede client, server e gesture sono distinte: i pin ripetuti su
schede diverse non sono condivisi fisicamente. Le tabelle non specificano i
numeri dei connettori, né sostituiscono lo schema di alimentazione dei moduli.

