#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "ArduinoJson.h"
#include "MsgPack.h"
#include "MessageProtocolDef.h"

typedef std::function<void()> ControlCommandCallback;

class CommandInterpreter {
    public:
        CommandInterpreter(Config *config, Floower *floower);
        uint16_t run(
            const uint16_t type,
            const char *payload,
            const uint16_t payloadLength,
            char *responsePayload = nullptr,
            uint16_t *responseLength = nullptr
        );
        void onControlCommand(ControlCommandCallback callback);
        void enableBluetooth();
        void disbleBluetooth();
        

    private:
        StaticJsonDocument<MAX_MESSAGE_PAYLOAD_BYTES> jsonPayload;  
        MsgPack::Unpacker payloadUnpacker;
        ControlCommandCallback controlCommandCallback;

        Config *config;
        Floower *floower;

        void fireControlCommandCallback(); 
};