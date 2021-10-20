#pragma once

#include "Arduino.h"
#include "Config.h"
#include "WiFi.h"
#include "AsyncTCP.h"

#define WIFI_MAX_PAYLOAD_BYTES 255

struct WifiMessageHeader {
    uint16_t type;
    uint16_t id;
    uint16_t length;
} __attribute__((packed));

class WifiConnect {
    public:
        WifiConnect(Config *config);
        void setup();
        void loop();
        void start();
        void stop();
        void runOTAUpdate();

    private:
        void connect();
        void request();

        bool readMessage();
        int8_t readMessageHeader(WifiMessageHeader& header);

        void sendAuthorization();
        uint16_t sendMessage(const uint16_t type, const char* payload, const size_t payloadSize);

        void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
        void onWifiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
        void onWifiGotIp(WiFiEvent_t event, WiFiEventInfo_t info);

        void onSocketData(char *data, size_t len);
        void onSocketConnected();
        void onSocketDisconnected();

        Config *config;
        AsyncClient *client;

        unsigned long reconnectTime = 0;
        uint16_t messageIdCounter = 1;
        uint8_t state;
        uint16_t authorizationMessageId;
        char payloadBuffer[WIFI_MAX_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
};
