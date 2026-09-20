#include "gesture_link.h"

#include "ch.h"
#include "hal.h"

#include <stddef.h>
#include <stdint.h>

#define GESTURE_LINK_TX_LINE             PAL_LINE(GPIOC, 1U)
#define GESTURE_LINK_RX_LINE             PAL_LINE(GPIOC, 0U)
#define GESTURE_LINK_GPIO_AF             8U

#define GESTURE_LINK_THREAD_STACK        512U
#define GESTURE_LINK_THREAD_PRIORITY     (NORMALPRIO + 2)

#define GESTURE_LINK_RX_TIMEOUT          TIME_MS2I(500)
#define GESTURE_LINK_TX_TIMEOUT          TIME_MS2I(100)
#define GESTURE_LINK_DUPLICATE_WINDOW    TIME_MS2I(2000)

static THD_WORKING_AREA(
    gesture_link_thread_wa,
    GESTURE_LINK_THREAD_STACK
);

static gesture_link_callback_t command_callback;
static bool receiver_started;

static bool last_frame_valid;
static uint16_t last_sequence;
static gesture_command_t last_command;
static systime_t last_frame_time;

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

static bool send_ack(const gesture_frame_t *received_frame) {
    uint8_t response[GESTURE_PROTOCOL_FRAME_SIZE];

    gesture_protocol_encode(
        GESTURE_FRAME_ACK,
        received_frame->command,
        received_frame->sequence,
        response
    );

    return write_complete(
        response,
        sizeof(response),
        GESTURE_LINK_TX_TIMEOUT
    );
}

static bool frame_is_duplicate(const gesture_frame_t *frame) {
    if (!last_frame_valid) {
        return false;
    }

    if (frame->sequence != last_sequence ||
        frame->command != last_command) {
        return false;
    }

    return chVTTimeElapsedSinceX(last_frame_time) <
           GESTURE_LINK_DUPLICATE_WINDOW;
}

static void remember_frame(const gesture_frame_t *frame) {
    last_sequence = frame->sequence;
    last_command = frame->command;
    last_frame_time = chVTGetSystemTimeX();
    last_frame_valid = true;
}

static THD_FUNCTION(gesture_link_thread, argument) {
    gesture_protocol_parser_t parser;
    gesture_frame_t frame;
    uint8_t byte;

    (void)argument;

    chRegSetThreadName("gesture-link-rx");
    gesture_protocol_parser_init(&parser);

    while (true) {
        msg_t result = sioSynchronizeRX(
            &LPSIOD1,
            GESTURE_LINK_RX_TIMEOUT
        );

        if (result == MSG_TIMEOUT) {
            continue;
        }

        if (result != MSG_OK) {
            (void)sioGetAndClearErrors(&LPSIOD1);
            gesture_protocol_parser_init(&parser);
            chThdSleepMilliseconds(10);
            continue;
        }

        while (sioAsyncRead(&LPSIOD1, &byte, 1U) == 1U) {
            if (!gesture_protocol_parser_push(
                    &parser,
                    byte,
                    &frame)) {
                continue;
            }

            if (frame.type != GESTURE_FRAME_COMMAND) {
                continue;
            }

            /*
             * L'ACK viene inviato anche per un duplicato. In questo modo
             * la board gesture può fermare le ritrasmissioni senza far
             * eseguire due volte lo stesso comando.
             */
            (void)send_ack(&frame);

            if (frame_is_duplicate(&frame)) {
                continue;
            }

            remember_frame(&frame);

            if (command_callback != NULL) {
                command_callback(frame.command);
            }
        }
    }
}

bool gesture_link_start(gesture_link_callback_t callback) {
    msg_t status;
    thread_t *thread;

    if (receiver_started) {
        return true;
    }

    if (callback == NULL) {
        return false;
    }

    palSetLineMode(
        GESTURE_LINK_TX_LINE,
        PAL_MODE_ALTERNATE(GESTURE_LINK_GPIO_AF)
    );

    palSetLineMode(
        GESTURE_LINK_RX_LINE,
        PAL_MODE_ALTERNATE(GESTURE_LINK_GPIO_AF)
    );

    status = sioStart(&LPSIOD1, NULL);

    if (status != MSG_OK) {
        return false;
    }

    command_callback = callback;
    last_frame_valid = false;

    thread = chThdCreateStatic(
        gesture_link_thread_wa,
        sizeof(gesture_link_thread_wa),
        GESTURE_LINK_THREAD_PRIORITY,
        gesture_link_thread,
        NULL
    );

    if (thread == NULL) {
        sioStop(&LPSIOD1);
        command_callback = NULL;
        return false;
    }

    receiver_started = true;
    return true;
}
