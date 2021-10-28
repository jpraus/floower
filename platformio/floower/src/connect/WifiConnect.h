#pragma once

#include "ArduinoJson.h"
#include "MsgPack.h"
#include "Arduino.h"
#include "Config.h"
#include "MessageProtocolDef.h"
#include "hardware/Floower.h"
#include "WiFi.h"
#include "AsyncTCP.h"
#include "CommandInterpreter.h"

class WifiConnect {
    public:
        WifiConnect(Config *config, CommandInterpreter *cmdInterpreter);
        void setup();
        void loop();
        void enable();
        void disable();
        bool isEnabled();
        void reconnect();

        void runOTAUpdate();

    private:
        void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
        void onWifiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
        void onWifiGotIp(WiFiEvent_t event, WiFiEventInfo_t info);

        void socketReconnect();
        void onSocketData(char *data, size_t len);
        void onSocketConnected();
        void onSocketDisconnected();

        void sendAuthorization();

        void handleReceivedMessage();
        void receiveMessage(char *data, size_t len);
        uint16_t sendRequest(const uint16_t type, const char* payload, const size_t payloadSize);
        void sendRequest(const uint16_t type, const uint16_t id, const char* payload, const size_t payloadSize);
        void sendMessage(const uint16_t type, const uint16_t id, const char* payload, const size_t payloadSize);

        Config *config;
        CommandInterpreter *cmdInterpreter;
        AsyncClient *client;
        bool enabled = false;
        bool wifiOn = false;

        unsigned long reconnectTime = 0;
        uint16_t messageIdCounter = 1;
        uint8_t state;
        uint16_t authorizationMessageId;
        char sendBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string

        unsigned long receiveTime = 0; // time when reply should be received
        volatile bool received = false;
        MessageHeader receivedMessage; 
        char receiveBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
};
