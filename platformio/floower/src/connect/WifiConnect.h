#pragma once

#define ARDUINOJSON_VERSION 1

#include "ArduinoJson.h"
#include "MsgPack.h"
#include "Arduino.h"
#include "Config.h"
#include "MessageProtocolDef.h"
#include "hardware/Floower.h"
#include "WiFi.h"
#include "AsyncTCP.h"

struct WifiMessageHeader {
    uint16_t type;
    uint16_t id;
    uint16_t length;
} __attribute__((packed));

#define MAX_MESSAGE_PAYLOAD_BYTES 255

class WifiConnect {
    public:
        WifiConnect(Config *config, Floower *floower);
        void setup();
        void loop();
        void start();
        void stop();
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
        uint16_t sendMessage(const uint16_t type, const char* payload, const size_t payloadSize);

        Config *config;
        Floower *floower;
        AsyncClient *client;
        bool enabled = false;

        unsigned long reconnectTime = 0;
        uint16_t messageIdCounter = 1;
        uint8_t state;
        uint16_t authorizationMessageId;
        char sendBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string

        unsigned long receiveTime = 0; // time when reply should be received
        volatile bool received = false;
        WifiMessageHeader receivedMessage; 
        char receiveBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
        StaticJsonDocument<MAX_MESSAGE_PAYLOAD_BYTES> jsonPayload;  

        MsgPack::Unpacker payloadUnpacker;
};
