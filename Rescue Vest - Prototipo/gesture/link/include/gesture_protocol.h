#ifndef GESTURE_PROTOCOL_H
#define GESTURE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GESTURE_PROTOCOL_MAGIC_0       0xA5U
#define GESTURE_PROTOCOL_MAGIC_1       0x5AU
#define GESTURE_PROTOCOL_VERSION       0x01U
#define GESTURE_PROTOCOL_FRAME_SIZE    8U

typedef enum {
    GESTURE_COMMAND_NONE = 0,
    GESTURE_COMMAND_ACTIVATE = 1,
    GESTURE_COMMAND_SOS = 2,
    GESTURE_COMMAND_SAFE_ZONE = 3
} gesture_command_t;

typedef enum {
    GESTURE_FRAME_COMMAND = 1,
    GESTURE_FRAME_ACK = 2
} gesture_frame_type_t;

typedef struct {
    gesture_frame_type_t type;
    gesture_command_t command;
    uint16_t sequence;
} gesture_frame_t;

typedef struct {
    uint8_t data[GESTURE_PROTOCOL_FRAME_SIZE];
    size_t index;
} gesture_protocol_parser_t;

bool gesture_protocol_command_valid(gesture_command_t command);

void gesture_protocol_encode(
    gesture_frame_type_t type,
    gesture_command_t command,
    uint16_t sequence,
    uint8_t output[GESTURE_PROTOCOL_FRAME_SIZE]
);

bool gesture_protocol_decode(
    const uint8_t input[GESTURE_PROTOCOL_FRAME_SIZE],
    gesture_frame_t *frame
);

void gesture_protocol_parser_init(gesture_protocol_parser_t *parser);

bool gesture_protocol_parser_push(
    gesture_protocol_parser_t *parser,
    uint8_t byte,
    gesture_frame_t *frame
);

#endif
