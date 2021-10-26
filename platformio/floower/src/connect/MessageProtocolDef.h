#pragma once

#include "Arduino.h"
#include "NeoPixelBus.h"
#include "MsgPack.h"

enum MessageType {
    // response statuses (0-15)
    STATUS_OK                   = 0,
    STATUS_ERROR                = 1,
    STATUS_UNAUTHORIZED         = 2,
    STATUS_UNSUPPORTED          = 3,

    // protocol commands (16-63)
    PROTOCOL_AUTH               = 16, // authorize the connection with server by sending a secure token
    PROTOCOL_WRITE_WIFI         = 17, // setup wifi SSID and password setup
    PROTOCOL_WRITE_TOKEN        = 18, // setup token for AUTH command
    PROTOCOL_READ_STATUS        = 19,  // return status of the device/connection (WiFi,BT,Authorized,..)

    // device commands (64+)
    CMD_WRITE_PETALS            = 64,
    CMD_WRITE_RGB_COLOR         = 65,
    //CMD_WRITE_HSB_COLOR       = 66,
    CMD_READ_STATE              = 67, // state of Floower (color, petals open level)
    CMD_PLAY_ANIMATION          = 68,
};

struct MessageHeader {
    uint16_t type;
    uint16_t id;
    uint16_t length;
} __attribute__((packed));

#define MAX_MESSAGE_PAYLOAD_BYTES 255