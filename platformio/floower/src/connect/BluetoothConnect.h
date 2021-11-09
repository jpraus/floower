#pragma once

#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "Config.h"
#include "hardware/Floower.h"
#include "CommandProtocol.h"

#define STATE_TRANSITION_MODE_BIT_COLOR 0
#define STATE_TRANSITION_MODE_BIT_PETALS 1 // when this bit is set, the VALUE parameter means open level of petals (0-100%)
#define STATE_TRANSITION_MODE_BIT_ANIMATION 2 // when this bit is set, the VALUE parameter means ID of animation

class BluetoothConnect {
    public:
        BluetoothConnect(Floower *floower, Config *config, CommandProtocol *cmdProtocol);
        void enable();
        void disable();
        void updateFloowerState(int8_t petalsOpenLevel, HsbColor hsbColor);
        void updateStatusData(uint8_t batteryLevel, bool batteryCharging, uint8_t wifiStatus);
        bool isConnected();
        void reloadConfig();

    private:
        void init();
        void startAdvertising();
        void stopAdvertising();
        String md5(String value);
        
        Floower *floower;
        Config *config;
        CommandProtocol *cmdProtocol;
        BLEServer *server = nullptr;
        BLEService *commandService = nullptr;
        BLEService *connectService = nullptr;
        BLEService *configService = nullptr;
        BLEService *batteryService = nullptr;

        bool deviceConnected = false;
        uint16_t connectionId;
        bool enabled = false;
        bool advertising = false;
        bool initialized = false;
        char receiveBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
        char responseBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
        StaticJsonDocument<MAX_MESSAGE_PAYLOAD_BYTES> jsonPayload;  

        BLECharacteristic* createROCharacteristics(BLEService *service, const char *uuid, const char *value);

        // BLE server->client command interface
        class CommandCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                CommandCharacteristicsCallbacks(BluetoothConnect* bluetoothConnect) : bluetoothConnect(bluetoothConnect) {};
            private:
                BluetoothConnect* bluetoothConnect ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE server callbacks impl
        class ServerCallbacks : public BLEServerCallbacks {
            public:
                ServerCallbacks(BluetoothConnect* bluetoothConnect) : bluetoothConnect(bluetoothConnect) {};
            private:
                BluetoothConnect* bluetoothConnect ;
                void onConnect(BLEServer* server);
                void onDisconnect(BLEServer* server);
        };
};