#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "esp8266_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Dimensione massima di un pacchetto di controllo MQTT completo. */
#define MQTT_PACKET_MAX 768U

typedef struct {
  /* Trasporto a byte realizzato tramite la connessione TCP trasparente ESP. */
  esp8266_transport_t *transport;
  /* Identificatore da assegnare alla prossima nuova pubblicazione QoS 1. */
  uint16_t next_packet_id;
  /* Identificatore mantenuto finche Mosquitto non restituisce PUBACK. */
  uint16_t pending_publish_id;
  /* Contatore di inattivita, a passi di un secondo, per il keep-alive. */
  uint16_t idle_seconds;
  /* Stato del controllo di presenza MQTT. */
  bool ping_outstanding;
  /* Impone di ritentare lo stesso SOS con DUP e lo stesso Packet ID. */
  bool publish_pending;
  /* Buffer statico: il livello MQTT non effettua allocazioni nello heap. */
  uint8_t packet[MQTT_PACKET_MAX];
} mqtt_client_t;

/* Inizializza lo stato senza aprire una connessione di rete. */
void mqtt_client_object_init(mqtt_client_t *client,
                             esp8266_transport_t *transport);

/* Invia CONNECT MQTT 3.1.1 e valida il CONNACK del broker. */
bool mqtt_client_open(mqtt_client_t *client);

/* Elabora un pacchetto/timeout e mantiene lo stato del keep-alive MQTT. */
bool mqtt_client_yield(mqtt_client_t *client);

/*
 * Pubblica un SOS con QoS 1 e RETAIN disabilitato, poi attende PUBACK.
 *
 * Se la funzione restituisce false dopo avere iniziato l'invio, la richiesta
 * resta pendente. La chiamata successiva deve ricevere lo stesso topic e lo
 * stesso payload: il pacchetto verra ritentato con DUP e lo stesso Packet ID.
 */
bool mqtt_client_publish_qos1(mqtt_client_t *client,
                              const char *topic,
                              const uint8_t *payload,
                              size_t payload_length);

/* Permette al livello applicativo di non sostituire un SOS non confermato. */
bool mqtt_client_publish_is_pending(const mqtt_client_t *client);

#endif
