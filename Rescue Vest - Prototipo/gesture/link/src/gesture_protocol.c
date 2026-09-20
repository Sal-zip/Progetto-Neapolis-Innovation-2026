#include "gesture_protocol.h"

static uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0U;
    size_t i;
    uint8_t bit;

    for (i = 0U; i < length; ++i) {
        crc ^= data[i];

        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & 0x80U) != 0U) {
                crc = (uint8_t)((crc << 1U) ^ 0x07U);
            } else {
                crc <<= 1U;
            }
        }
    }

    return crc;
}

bool gesture_protocol_command_valid(gesture_command_t command) {
    return command == GESTURE_COMMAND_ACTIVATE ||
           command == GESTURE_COMMAND_SOS ||
           command == GESTURE_COMMAND_SAFE_ZONE;
}

void gesture_protocol_encode(
    gesture_frame_type_t type,
    gesture_command_t command,
    uint16_t sequence,
    uint8_t output[GESTURE_PROTOCOL_FRAME_SIZE]
) {
    if (output == NULL) {
        return;
    }

    output[0] = GESTURE_PROTOCOL_MAGIC_0;
    output[1] = GESTURE_PROTOCOL_MAGIC_1;
    output[2] = GESTURE_PROTOCOL_VERSION;
    output[3] = (uint8_t)type;
    output[4] = (uint8_t)(sequence >> 8U);
    output[5] = (uint8_t)(sequence & 0xFFU);
    output[6] = (uint8_t)command;
    output[7] = calculate_crc8(&output[2], 5U);
}

bool gesture_protocol_decode(
    const uint8_t input[GESTURE_PROTOCOL_FRAME_SIZE],
    gesture_frame_t *frame
) {
    uint16_t sequence;
    gesture_frame_type_t type;
    gesture_command_t command;

    if (input == NULL || frame == NULL) {
        return false;
    }

    if (input[0] != GESTURE_PROTOCOL_MAGIC_0 ||
        input[1] != GESTURE_PROTOCOL_MAGIC_1 ||
        input[2] != GESTURE_PROTOCOL_VERSION) {
        return false;
    }

    if (input[7] != calculate_crc8(&input[2], 5U)) {
        return false;
    }

    type = (gesture_frame_type_t)input[3];
    command = (gesture_command_t)input[6];

    if (type != GESTURE_FRAME_COMMAND &&
        type != GESTURE_FRAME_ACK) {
        return false;
    }

    if (!gesture_protocol_command_valid(command)) {
        return false;
    }

    sequence = ((uint16_t)input[4] << 8U) |
               (uint16_t)input[5];

    frame->type = type;
    frame->command = command;
    frame->sequence = sequence;

    return true;
}

void gesture_protocol_parser_init(gesture_protocol_parser_t *parser) {
    if (parser != NULL) {
        parser->index = 0U;
    }
}

bool gesture_protocol_parser_push(
    gesture_protocol_parser_t *parser,
    uint8_t byte,
    gesture_frame_t *frame
) {
    bool complete;

    if (parser == NULL || frame == NULL) {
        return false;
    }

    if (parser->index == 0U) {
        if (byte == GESTURE_PROTOCOL_MAGIC_0) {
            parser->data[0] = byte;
            parser->index = 1U;
        }

        return false;
    }

    if (parser->index == 1U) {
        if (byte == GESTURE_PROTOCOL_MAGIC_1) {
            parser->data[1] = byte;
            parser->index = 2U;
        } else if (byte == GESTURE_PROTOCOL_MAGIC_0) {
            parser->data[0] = byte;
            parser->index = 1U;
        } else {
            parser->index = 0U;
        }

        return false;
    }

    parser->data[parser->index++] = byte;

    if (parser->index < GESTURE_PROTOCOL_FRAME_SIZE) {
        return false;
    }

    complete = gesture_protocol_decode(parser->data, frame);
    parser->index = 0U;

    return complete;
}
