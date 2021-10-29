#pragma once

#include "Arduino.h"
#include "NeoPixelBus.h"
#include "MsgPack.h"

enum CommandType {
    // response statuses (0-15)
    STATUS_OK                   = 0,
    STATUS_ERROR                = 1,
    STATUS_UNAUTHORIZED         = 2,
    STATUS_UNSUPPORTED          = 3,

    // protocol commands (16-63)
    PROTOCOL_AUTH               = 16, // authorize the connection with server by sending a secure token

    // device commands (64+)
    CMD_WRITE_PETALS            = 64,
    CMD_WRITE_RGB_COLOR         = 65,
    //CMD_WRITE_HSB_COLOR       = 66,
    CMD_WRITE_STATE             = 67, // setting both color and petals open level
    CMD_READ_STATE              = 68, // state of Floower (color, petals open level)
    CMD_PLAY_ANIMATION          = 69,
    CMD_RUN_OTA                 = 70,
    CMD_WRITE_WIFI              = 71,
    CMD_READ_WIFI               = 72,
    CMD_WRITE_PERSONIFICATION   = 73,
    CMD_READ_PERSONIFICATION    = 74,
    CMD_WRITE_COLOR_SCHEME      = 75,
    CMD_READ_COLOR_SCHEME       = 76,
};

struct CommandMessageHeader {
    uint16_t type;
    uint16_t id;
    uint16_t length;
} __attribute__((packed));

#define MAX_MESSAGE_PAYLOAD_BYTES 255