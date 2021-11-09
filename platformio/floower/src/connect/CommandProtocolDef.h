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
    PROTOCOL_STATUS             = 17, // heartbeat status

    // device commands (64+)
    CMD_WRITE_PETALS            = 64,
    CMD_WRITE_RGB_COLOR         = 65,
    //CMD_WRITE_HSB_COLOR       = 66,
    CMD_WRITE_STATE             = 67, // setting both color and petals open level
    CMD_READ_STATE              = 68, // state of Floower (color, petals open level)
    CMD_PLAY_ANIMATION          = 69,
    CMD_RUN_OTA_UPDATE          = 70,
    CMD_WRITE_WIFI              = 71,
    CMD_READ_WIFI               = 72,
    CMD_READ_WIFI_STATUS        = 73,
    CMD_WRITE_NAME              = 74,
    CMD_WRITE_CUSTOMIZATION     = 75,
    CMD_READ_CUSTOMIZATION      = 76,
    CMD_WRITE_COLOR_SCHEME      = 77,
    CMD_READ_COLOR_SCHEME       = 78,
    CMD_READ_DEVICE_INFO        = 79 // serial number, name, hw revision, fw revision, model name
};

struct CommandMessageHeader {
    uint16_t type;
    uint16_t id;
    uint16_t length;
} __attribute__((packed));

#define MAX_MESSAGE_PAYLOAD_BYTES 255