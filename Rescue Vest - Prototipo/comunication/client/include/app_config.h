#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * Configurazione del client SOS, mantenuta intenzionalmente parallela a
 * RT-STM32G474RE-SOS-SERVER/lib/app_config.h.
 */

#define APP_WIFI_SSID                       "RESCUE_VEST_WIFI_ESEMPIO"
#define APP_WIFI_PASSWORD                   "PASSWORD_WIFI_DI_ESEMPIO"

/* MQTT 3.1.1 su TCP non cifrato, come nel server fornito. */
#define APP_MQTT_BROKER_HOST                "10.162.161.135"
#define APP_MQTT_BROKER_PORT                1883U

/* Deve essere stabile e non deve coincidere con il Client ID del server. */
#define APP_MQTT_CLIENT_ID                  "neapolis-sos-client-01"

#define APP_MQTT_USERNAME                   ""
#define APP_MQTT_PASSWORD                   ""

/* Il client pubblica sullo stesso topic al quale si iscrive il server. */
#define APP_MQTT_SOS_TOPIC                  "neapolis/2026/sos/requests"

/* Telemetria periodica di GPS, PPG e MQ2. */
#define APP_MQTT_TELEMETRY_TOPIC            \
    "neapolis/2026/operators/telemetry"

/* Attivazione operatore e segnalazione zona sicura. */
#define APP_MQTT_STATUS_TOPIC               \
    "neapolis/2026/operators/status"

/* Periodo di acquisizione e pubblicazione telemetria. */
#define APP_TELEMETRY_PERIOD_MS             1000U

/* Età massima accettata per uno snapshot sensore. */
#define APP_SENSOR_MAX_AGE_MS               3000U

#define APP_MQTT_KEEP_ALIVE_SEC             30U
#define APP_UART_BAUD                       38400U

#define APP_DEVICE_ID                       "worker-device-01"

#endif
