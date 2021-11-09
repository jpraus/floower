#pragma once

#include "ArduinoJson.h"
#include "MsgPack.h"
#include "Arduino.h"
#include "Config.h"
#include "CommandProtocolDef.h"
#include "hardware/Floower.h"
#include "WiFi.h"
#include "AsyncTCP.h"
#include "CommandProtocol.h"

// network status
#define WIFI_STATUS_DISABLED 0
#define WIFI_STATUS_NOT_CONFIGURED 1
#define WIFI_STATUS_NOT_CONNECTED 2
#define WIFI_STATUS_FLOUD_UNAUTHORIZED 3
#define WIFI_STATUS_FLOUD_CONNECTED 4

class WifiConnect {
    public:
        WifiConnect(Config *config, CommandProtocol *cmdProtocol);
        void setup();
        void loop();
        void enable();
        void disable();
        void reconnect();
        void updateFloowerState(int8_t petalsOpenLevel, HsbColor hsbColor);
        void updateStatusData(uint8_t batteryLevel, bool batteryCharging);
        bool isEnabled();
        bool isConnected();
        uint8_t getStatus();
        void startOTAUpdate(String firmwareUrl);
        bool isOTAUpdateRunning();

    private:
        void onWifiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
        void onWifiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
        void onWifiGotIp(WiFiEvent_t event, WiFiEventInfo_t info);

        void ensureClient();
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

        void requestOTAUpdateData();
        void receiveOTAUpdateData(char *data, size_t len);
        void finalizeOTAUpdate();

        Config *config;
        CommandProtocol *cmdProtocol;
        AsyncClient *client;
        bool enabled = false;
        bool wifiOn = false;
        bool wifiConnected = false;

        uint8_t mode;
        uint8_t state;

        unsigned long reconnectTime = 0;
        uint16_t messageIdCounter = 1;
        uint16_t authorizationMessageId;
        bool authorizationFailed = false;
        char sendBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string

        unsigned long receiveTime = 0; // time when reply should be received
        volatile bool received = false;
        CommandMessageHeader receivedMessage; 
        char receiveBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string

        String updateFirmwareHost;
        String updateFirmwarePath;
        size_t updateDataLength;
        size_t updateReceivedBytes;
};
