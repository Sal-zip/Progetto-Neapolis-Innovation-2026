#ifndef ESP8266_TRANSPORT_H
#define ESP8266_TRANSPORT_H

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  /* USART collegata all'ESP8266, usata esclusivamente dal thread MQTT. */
  SerialDriver *serial;
  /* Stream diagnostico opzionale, normalmente la porta COM virtuale ST-LINK. */
  BaseSequentialStream *debug;
  /* Stato locale della modalita trasparente ESP-AT. */
  bool transparent_mode;
} esp8266_transport_t;

/* Associa un oggetto di trasporto non inizializzato agli stream ChibiOS. */
void esp8266_transport_object_init(esp8266_transport_t *transport,
                                   SerialDriver *serial,
                                   BaseSequentialStream *debug);

/* Collega il Wi-Fi, apre il socket TCP del broker ed entra in modalita trasparente. */
bool esp8266_transport_connect(esp8266_transport_t *transport);

/* Esce dalla modalita trasparente e ordina a ESP-AT di chiudere il socket TCP. */
void esp8266_transport_disconnect(esp8266_transport_t *transport);

/* Scrive byte MQTT grezzi sul flusso TCP trasparente gia stabilito. */
bool esp8266_transport_write(esp8266_transport_t *transport,
                             const uint8_t *data, size_t length);

/* Legge byte MQTT grezzi con timeout ChibiOS; zero indica assenza di dati. */
size_t esp8266_transport_read(esp8266_transport_t *transport,
                              uint8_t *data, size_t length,
                              sysinterval_t timeout);

#endif
