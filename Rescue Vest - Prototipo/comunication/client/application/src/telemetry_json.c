#include "telemetry_json.h"

#include "app_config.h"
#include "chprintf.h"

#include <limits.h>
#include <string.h>

static const char *json_bool(bool value) {
  return value ? "true" : "false";
}

static int32_t coordinate_to_e7(double coordinate) {
  const double scaled = coordinate * 10000000.0;

  if (scaled > (double)INT32_MAX) {
    return INT32_MAX;
  }

  if (scaled < (double)INT32_MIN) {
    return INT32_MIN;
  }

  return (int32_t)scaled;
}

static uint32_t positive_float_to_milli(float value) {
  if (value <= 0.0f) {
    return 0U;
  }

  const float scaled = value * 1000.0f;

  if (scaled >= (float)UINT32_MAX) {
    return UINT32_MAX;
  }

  return (uint32_t)scaled;
}

static bool select_message_metadata(application_message_kind_t kind,
                                    const char **topic,
                                    const char **type,
                                    const char **event) {
  if ((topic == NULL) || (type == NULL) || (event == NULL)) {
    return false;
  }

  switch (kind) {
    case APPLICATION_MESSAGE_TELEMETRY:
      *topic = APP_MQTT_TELEMETRY_TOPIC;
      *type = "telemetry";
      *event = "sample";
      return true;

    case APPLICATION_MESSAGE_ACTIVATED:
      *topic = APP_MQTT_STATUS_TOPIC;
      *type = "operator_status";
      *event = "activated";
      return true;

    case APPLICATION_MESSAGE_MANUAL_SOS:
      *topic = APP_MQTT_SOS_TOPIC;
      *type = "sos";
      *event = "manual_gesture";
      return true;

    case APPLICATION_MESSAGE_PPG_SOS:
      *topic = APP_MQTT_SOS_TOPIC;
      *type = "sos";
      *event = "ppg_emergency";
      return true;

    case APPLICATION_MESSAGE_MQ2_SOS:
      *topic = APP_MQTT_SOS_TOPIC;
      *type = "sos";
      *event = "gas_emergency";
      return true;

    case APPLICATION_MESSAGE_ZONE_SAFE:
      *topic = APP_MQTT_STATUS_TOPIC;
      *type = "operator_status";
      *event = "zone_safe";
      return true;

    default:
      return false;
  }
}

bool telemetry_json_build(application_message_kind_t kind,
                          uint32_t sequence,
                          const telemetry_snapshot_t *snapshot,
                          application_message_t *message) {
  if ((snapshot == NULL) || (message == NULL) ||
      (APP_DEVICE_ID[0] == '\0')) {
    return false;
  }

  const char *topic;
  const char *type;
  const char *event;

  if (!select_message_metadata(kind, &topic, &type, &event)) {
    return false;
  }

  memset(message, 0, sizeof(*message));

  const int topic_length =
      chsnprintf(message->topic, sizeof(message->topic), "%s", topic);

  if ((topic_length < 0) ||
      ((size_t)topic_length >= sizeof(message->topic))) {
    return false;
  }

  const int event_id_length =
      chsnprintf(message->event_id,
                 sizeof(message->event_id),
                 "%s-%08lu",
                 APP_DEVICE_ID,
                 (unsigned long)sequence);

  if ((event_id_length < 0) ||
      ((size_t)event_id_length >= sizeof(message->event_id))) {
    return false;
  }

  const int32_t latitude_e7 =
      coordinate_to_e7(snapshot->gps.latitude);

  const int32_t longitude_e7 =
      coordinate_to_e7(snapshot->gps.longitude);

  const uint32_t speed_milli_knots =
      positive_float_to_milli(snapshot->gps.speed_knots);

  const uint32_t heading_milli_degrees =
      positive_float_to_milli(snapshot->gps.heading);

  const int payload_length =
      chsnprintf(
          (char *)message->payload,
          sizeof(message->payload),

          "{"
            "\"schemaVersion\":1,"
            "\"type\":\"%s\","
            "\"event\":\"%s\","
            "\"deviceId\":\"%s\","
            "\"eventId\":\"%s\","
            "\"sequence\":%lu,"
            "\"uptimeMs\":%lu,"

            "\"gps\":{"
              "\"fresh\":%s,"
              "\"valid\":%s,"
              "\"latitudeE7\":%ld,"
              "\"longitudeE7\":%ld,"
              "\"speedMilliKnots\":%lu,"
              "\"headingMilliDegrees\":%lu"
            "},"

            "\"ppg\":{"
              "\"fresh\":%s,"
              "\"valid\":%s,"
              "\"bpm\":%lu,"
              "\"redRaw\":%lu,"
              "\"infraredRaw\":%lu,"
              "\"status\":%u,"
              "\"emergency\":%s"
            "},"

            "\"mq2\":{"
              "\"fresh\":%s,"
              "\"valid\":%s,"
              "\"rawAdc\":%u,"
              "\"status\":%u,"
              "\"emergency\":%s"
            "}"
          "}",

          type,
          event,
          APP_DEVICE_ID,
          message->event_id,
          (unsigned long)sequence,
          (unsigned long)snapshot->uptime_ms,

          json_bool(snapshot->gps_fresh),
          json_bool(snapshot->gps.is_valid),
          (long)latitude_e7,
          (long)longitude_e7,
          (unsigned long)speed_milli_knots,
          (unsigned long)heading_milli_degrees,

          json_bool(snapshot->ppg_fresh),
          json_bool(snapshot->ppg.valid),
          (unsigned long)snapshot->ppg.heart_rate_bpm,
          (unsigned long)snapshot->ppg.red_raw,
          (unsigned long)snapshot->ppg.infrared_raw,
          (unsigned)snapshot->ppg.status,
          json_bool(snapshot->ppg.emergency),

          json_bool(snapshot->mq2_fresh),
          json_bool(snapshot->mq2.valid),
          (unsigned)snapshot->mq2.raw_adc,
          (unsigned)snapshot->mq2.status,
          json_bool(snapshot->mq2.emergency));

  if ((payload_length < 0) ||
      ((size_t)payload_length > APPLICATION_PAYLOAD_MAX)) {
    message->payload_length = 0U;
    return false;
  }

  message->payload_length = (size_t)payload_length;
  return true;
}
