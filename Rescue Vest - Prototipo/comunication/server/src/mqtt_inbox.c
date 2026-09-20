#include "mqtt_inbox.h"

#include <string.h>

/* Le mailbox trasferiscono la proprieta degli slot senza usare lo heap. */
static mqtt_message_t slots[MQTT_INBOX_CAPACITY];
static mailbox_t free_mailbox;
static mailbox_t ready_mailbox;
static msg_t free_queue[MQTT_INBOX_CAPACITY];
static msg_t ready_queue[MQTT_INBOX_CAPACITY];

void mqtt_inbox_init(void) {
  chMBObjectInit(&free_mailbox, free_queue, MQTT_INBOX_CAPACITY);
  chMBObjectInit(&ready_mailbox, ready_queue, MQTT_INBOX_CAPACITY);
  for (size_t i = 0U; i < MQTT_INBOX_CAPACITY; ++i) {
    (void)chMBPostTimeout(&free_mailbox, (msg_t)i, TIME_IMMEDIATE);
  }
}

bool mqtt_inbox_try_put(const char *topic, const uint8_t *payload,
                        size_t payload_length, bool retained, bool duplicate) {
  const size_t topic_length = strlen(topic);
  msg_t index;
  if ((topic_length > MQTT_TOPIC_MAX) ||
      (payload_length > MQTT_PAYLOAD_MAX) ||
      (chMBFetchTimeout(&free_mailbox, &index, TIME_IMMEDIATE) != MSG_OK)) {
    return false;
  }

  mqtt_message_t *message = &slots[(size_t)index];
  memcpy(message->topic, topic, topic_length + 1U);
  memcpy(message->payload, payload, payload_length);
  message->payload[payload_length] = '\0';
  message->payload_length = payload_length;
  message->retained = retained;
  message->duplicate = duplicate;

  if (chMBPostTimeout(&ready_mailbox, index, TIME_IMMEDIATE) != MSG_OK) {
    (void)chMBPostTimeout(&free_mailbox, index, TIME_IMMEDIATE);
    return false;
  }
  return true;
}

mqtt_message_t *mqtt_inbox_take(void) {
  msg_t index;
  if (chMBFetchTimeout(&ready_mailbox, &index, TIME_INFINITE) != MSG_OK) {
    return NULL;
  }
  return &slots[(size_t)index];
}

void mqtt_inbox_release(mqtt_message_t *message) {
  if ((message == NULL) || (message < slots) ||
      (message >= slots + MQTT_INBOX_CAPACITY)) {
    return;
  }
  const msg_t index = (msg_t)(message - slots);
  (void)chMBPostTimeout(&free_mailbox, index, TIME_INFINITE);
}
