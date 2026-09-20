#ifndef MQTT_INBOX_H
#define MQTT_INBOX_H

#include "ch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Limiti fissi per mantenere deterministico l'uso della RAM. */
#define MQTT_INBOX_CAPACITY     8U
#define MQTT_TOPIC_MAX          127U
/* Lo schema completo del client arriva a circa 510 byte con i campi massimi;
 * il margine evita rifiuti se deviceId/eventId diventano leggermente piu lunghi. */
#define MQTT_PAYLOAD_MAX        639U

typedef struct {
  char topic[MQTT_TOPIC_MAX + 1U];
  uint8_t payload[MQTT_PAYLOAD_MAX + 1U];
  size_t payload_length;
  bool retained;
  bool duplicate;
} mqtt_message_t;

void mqtt_inbox_init(void);
bool mqtt_inbox_try_put(const char *topic, const uint8_t *payload,
                        size_t payload_length, bool retained, bool duplicate);
mqtt_message_t *mqtt_inbox_take(void);
void mqtt_inbox_release(mqtt_message_t *message);

#endif
