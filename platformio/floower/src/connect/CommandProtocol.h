#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "ArduinoJson.h"
#include "MsgPack.h"
#include "CommandProtocolDef.h"

typedef std::function<void()> ControlCommandCallback;
typedef std::function<void(String firmwareUrl)> RunOTAUpdateCallback;

class CommandProtocol {
    public:
        CommandProtocol(Config *config, Floower *floower);
        uint16_t run(
            const uint16_t type,
            const char *payload,
            const uint16_t payloadLength,
            char *responsePayload = nullptr,
            uint16_t *responseLength = nullptr
        );
        uint16_t sendStatus(const uint8_t batteryLevel, const bool charging, char *payload, uint16_t *payloadLength); // returns type of command that should be send
        uint16_t sendState(const int8_t petalsOpenLevel, const HsbColor hsbColor, char *payload, uint16_t *payloadLength); // returns type of command that should be send
        void onControlCommand(ControlCommandCallback callback);
        void onRunOTAUpdate(RunOTAUpdateCallback callback);
        void enableBluetooth();
        void disbleBluetooth();
        

    private:
        StaticJsonDocument<MAX_MESSAGE_PAYLOAD_BYTES> jsonPayload;  
        MsgPack::Unpacker payloadUnpacker;
        ControlCommandCallback controlCommandCallback;
        RunOTAUpdateCallback runOTAUpdateCallback;

        Config *config;
        Floower *floower;

        void fireControlCommandCallback(); 
};