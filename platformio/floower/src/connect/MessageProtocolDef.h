#pragma once

#include "Arduino.h"
#include "NeoPixelBus.h"
#include "MsgPack.h"

enum MessageType {
    // response statuses (0-15)
    STATUS_OK                   = 0,
    STATUS_ERROR                = 1,
    STATUS_UNAUTHORIZED         = 2,

    // protocol commands (16-63)
    PROTOCOL_AUTH               = 16, // authorize the connection with server by sending a secure token
    PROTOCOL_WRITE_WIFI         = 17, // setup wifi SSID and password setup
    PROTOCOL_WRITE_TOKEN        = 18, // setup token for AUTH command
    PROTOCOL_READ_STATUS        = 19,  // return status of the device/connection (WiFi,BT,Authorized,..)

    // device commands (64+)
    CMD_WRITE_PETALS            = 64,
    CMD_WRITE_COLOR             = 65
};

struct CommandPayloadColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint16_t time;

    RgbColor getColor() {
        return RgbColor(red, green, blue);
    }
} __attribute__((packed));

struct CommandPayloadPetals {
    uint8_t level;
    uint16_t time;
} __attribute__((packed));