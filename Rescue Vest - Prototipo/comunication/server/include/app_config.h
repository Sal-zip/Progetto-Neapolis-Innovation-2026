#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Configurazione locale del firmware ChibiOS server. */

#define APP_WIFI_SSID              "RESCUE_VEST_WIFI_ESEMPIO"
#define APP_WIFI_PASSWORD          "PASSWORD_WIFI_DI_ESEMPIO"

#define APP_MQTT_BROKER_HOST       "10.162.161.51"
#define APP_MQTT_BROKER_PORT       1883U
#define APP_MQTT_CLIENT_ID         "neapolis-sos-server-01"

#define APP_MQTT_USERNAME          ""
#define APP_MQTT_PASSWORD          ""

#define APP_MQTT_SOS_TOPIC         "neapolis/2026/sos/requests"
#define APP_MQTT_TELEMETRY_TOPIC   "neapolis/2026/operators/telemetry"
#define APP_MQTT_STATUS_TOPIC      "neapolis/2026/operators/status"
#define APP_MQTT_TOPIC_COUNT       3U

#define APP_MQTT_KEEP_ALIVE_SEC    30U
#define APP_UART_BAUD              38400U

#endif
