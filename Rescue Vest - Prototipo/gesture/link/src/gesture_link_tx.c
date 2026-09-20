#include "gesture_link.h"

#include "ch.h"
#include "hal.h"

#include <stddef.h>
#include <stdint.h>

#define GESTURE_LINK_TX_LINE          PAL_LINE(GPIOC, 1U)
#define GESTURE_LINK_RX_LINE          PAL_LINE(GPIOC, 0U)
#define GESTURE_LINK_GPIO_AF          8U

#define GESTURE_LINK_ACK_TIMEOUT      TIME_MS2I(200)
#define GESTURE_LINK_TX_TIMEOUT       TIME_MS2I(100)
#define GESTURE_LINK_RETRY_DELAY      TIME_MS2I(40)
#define GESTURE_LINK_MAX_RETRIES      3U
#define GESTURE_LINK_MAX_RX_BYTES     32U

static mutex_t gesture_link_mutex;
static uint16_t gesture_sequence;
static bool gesture_link_initialized;

static bool write_complete(
    const uint8_t *data,
    size_t length,
    sysinterval_t timeout
) {
    size_t offset = 0U;

    while (offset < length) {
        size_t written = sioAsyncWrite(
            &LPSIOD1,
            &data[offset],
            length - offset
        );

        offset += written;

        if (offset < length) {
            if (sioSynchronizeTX(&LPSIOD1, timeout) != MSG_OK) {
                return false;
            }
        }
    }

    return sioSynchronizeTXEnd(&LPSIOD1, timeout) == MSG_OK;
}

static bool read_byte(uint8_t *byte, sysinterval_t timeout) {
    if (byte == NULL) {
        return false;
    }

    if (sioSynchronizeRX(&LPSIOD1, timeout) != MSG_OK) {
        return false;
    }

    return sioAsyncRead(&LPSIOD1, byte, 1U) == 1U;
}

static bool wait_for_ack(
    uint16_t expected_sequence,
    gesture_command_t expected_command
) {
    gesture_protocol_parser_t parser;
    gesture_frame_t frame;
    uint8_t byte;
    unsigned int count;

    gesture_protocol_parser_init(&parser);

    for (count = 0U; count < GESTURE_LINK_MAX_RX_BYTES; ++count) {
        if (!read_byte(&byte, GESTURE_LINK_ACK_TIMEOUT)) {
            return false;
        }

        if (gesture_protocol_parser_push(&parser, byte, &frame)) {
            if (frame.type == GESTURE_FRAME_ACK &&
                frame.sequence == expected_sequence &&
                frame.command == expected_command) {
                return true;
            }
        }
    }

    return false;
}

bool gesture_link_tx_start(void) {
    msg_t status;

    if (gesture_link_initialized) {
        return true;
    }

    palSetLineMode(
        GESTURE_LINK_TX_LINE,
        PAL_MODE_ALTERNATE(GESTURE_LINK_GPIO_AF)
    );

    palSetLineMode(
        GESTURE_LINK_RX_LINE,
        PAL_MODE_ALTERNATE(GESTURE_LINK_GPIO_AF)
    );

    chMtxObjectInit(&gesture_link_mutex);

    status = sioStart(&LPSIOD1, NULL);

    if (status != MSG_OK) {
        return false;
    }

    gesture_sequence = 0U;
    gesture_link_initialized = true;

    return true;
}

bool gesture_link_send(gesture_command_t command) {
    uint8_t frame[GESTURE_PROTOCOL_FRAME_SIZE];
    uint16_t sequence;
    unsigned int attempt;
    bool acknowledged = false;

    if (!gesture_link_initialized ||
        !gesture_protocol_command_valid(command)) {
        return false;
    }

    chMtxLock(&gesture_link_mutex);

    ++gesture_sequence;

    if (gesture_sequence == 0U) {
        ++gesture_sequence;
    }

    sequence = gesture_sequence;

    gesture_protocol_encode(
        GESTURE_FRAME_COMMAND,
        command,
        sequence,
        frame
    );

    for (attempt = 0U;
         attempt < GESTURE_LINK_MAX_RETRIES;
         ++attempt) {

        if (write_complete(
                frame,
                sizeof(frame),
                GESTURE_LINK_TX_TIMEOUT) &&
            wait_for_ack(sequence, command)) {

            acknowledged = true;
            break;
        }

        (void)sioGetAndClearErrors(&LPSIOD1);
        chThdSleep(GESTURE_LINK_RETRY_DELAY);
    }

    chMtxUnlock(&gesture_link_mutex);

    return acknowledged;
}
