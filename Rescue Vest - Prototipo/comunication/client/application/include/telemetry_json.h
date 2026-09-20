#ifndef TELEMETRY_JSON_H
#define TELEMETRY_JSON_H

#include "telemetry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APPLICATION_TOPIC_MAX       96U
#define APPLICATION_EVENT_ID_MAX    64U
#define APPLICATION_PAYLOAD_MAX    511U

typedef enum {
  APPLICATION_MESSAGE_TELEMETRY = 0,
  APPLICATION_MESSAGE_ACTIVATED,
  APPLICATION_MESSAGE_MANUAL_SOS,
  APPLICATION_MESSAGE_PPG_SOS,
  APPLICATION_MESSAGE_MQ2_SOS,
  APPLICATION_MESSAGE_ZONE_SAFE
} application_message_kind_t;

typedef struct {
  char topic[APPLICATION_TOPIC_MAX + 1U];
  char event_id[APPLICATION_EVENT_ID_MAX + 1U];

  uint8_t payload[APPLICATION_PAYLOAD_MAX + 1U];
  size_t payload_length;
} application_message_t;

/*
 * Crea un messaggio JSON completo e immutabile.
 * Il messaggio può essere conservato fino al PUBACK MQTT.
 */
bool telemetry_json_build(application_message_kind_t kind,
                          uint32_t sequence,
                          const telemetry_snapshot_t *snapshot,
                          application_message_t *message);

#endif /* TELEMETRY_JSON_H */
