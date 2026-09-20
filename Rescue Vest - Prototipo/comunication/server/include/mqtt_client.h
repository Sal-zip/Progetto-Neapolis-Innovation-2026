#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "esp8266_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Dimensione massima di un pacchetto di controllo MQTT completo. */
#define MQTT_PACKET_MAX 768U

/*
 * La callback restituisce true solo dopo che l'applicazione ha accettato un
 * PUBLISH. Con QoS 1 il PUBACK parte soltanto quando essa restituisce true.
 */
typedef bool (*mqtt_publish_callback_t)(const char *topic,
                                        const uint8_t *payload,
                                        size_t payload_length,
                                        bool retained,
                                        bool duplicate);

typedef struct {
  /* Trasporto a byte realizzato tramite la connessione TCP trasparente ESP. */
  esp8266_transport_t *transport;
  /* Confine applicativo usato per accodare un messaggio in ingresso. */
  mqtt_publish_callback_t on_publish;
  /* Identificatore usato da SUBSCRIBE e dalle future pubblicazioni QoS 1. */
  uint16_t next_packet_id;
  /* Contatore di inattivita, a passi di un secondo, per il keep-alive. */
  uint16_t idle_seconds;
  /* Impedisce PINGREQ illimitati in assenza del corrispondente PINGRESP. */
  bool ping_outstanding;
  /* Buffer statico: il livello MQTT non effettua allocazioni nello heap. */
  uint8_t packet[MQTT_PACKET_MAX];
} mqtt_client_t;

/* Inizializza lo stato senza aprire una connessione di rete. */
void mqtt_client_object_init(mqtt_client_t *client,
                             esp8266_transport_t *transport,
                             mqtt_publish_callback_t callback);
/* Invia CONNECT, valida CONNACK, sottoscrive i topic e valida SUBACK. */
bool mqtt_client_open(mqtt_client_t *client);

/* Elabora un pacchetto/timeout e mantiene lo stato del keep-alive MQTT. */
bool mqtt_client_yield(mqtt_client_t *client);

#endif
