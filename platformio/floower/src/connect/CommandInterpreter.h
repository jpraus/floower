#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "ArduinoJson.h"
#include "MsgPack.h"
#include "MessageProtocolDef.h"

typedef std::function<void()> RemoteControlCallback;

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
        void onRemoteControl(RemoteControlCallback callback);
        void enableBluetooth();
        void disbleBluetooth();
        

    private:
        StaticJsonDocument<MAX_MESSAGE_PAYLOAD_BYTES> jsonPayload;  
        MsgPack::Unpacker payloadUnpacker;
        RemoteControlCallback remoteControlCallback;

        Config *config;
        Floower *floower;
};