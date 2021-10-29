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

typedef std::function<void()> BluetoothControlRemoteControlCallback;

class BluetoothConnect {
    public:
        BluetoothConnect(Floower *floower, Config *config, CommandProtocol *cmdProtocol);
        void enable();
        void disable();
        void setBatteryLevel(uint8_t level, bool charging);
        bool isConnected();
        void onRemoteControl(BluetoothControlRemoteControlCallback callback);

    private:
        void init();
        void startAdvertising();
        void stopAdvertising();
        
        Floower *floower;
        Config *config;
        CommandProtocol *cmdProtocol;
        BluetoothControlRemoteControlCallback remoteControlCallback;
        BLEServer *server = nullptr;
        BLEService *floowerService = nullptr;
        BLEService *batteryService = nullptr;

        bool deviceConnected = false;
        uint16_t connectionId;
        bool advertising = false;
        bool initialized = false;
        char receiveBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
        char responseBuffer[MAX_MESSAGE_PAYLOAD_BYTES + 1]; // extra space for 0 terminating string
        StaticJsonDocument<MAX_MESSAGE_PAYLOAD_BYTES> jsonPayload;  

        BLECharacteristic* createROCharacteristics(BLEService *service, const char *uuid, const char *value);

        // BLE state characteristics callback
        class StateChangeCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                StateChangeCharacteristicsCallbacks(BluetoothConnect* bluetoothConnect) : bluetoothConnect(bluetoothConnect) {};
            private:
                BluetoothConnect* bluetoothConnect ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE name characteristics callback
        class NameCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                NameCharacteristicsCallbacks(BluetoothConnect* bluetoothConnect) : bluetoothConnect(bluetoothConnect) {};
            private:
                BluetoothConnect* bluetoothConnect ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE color scheme characteristics callback
        class ColorsSchemeCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                ColorsSchemeCharacteristicsCallbacks(BluetoothConnect* bluetoothConnect) : bluetoothConnect(bluetoothConnect) {};
            private:
                BluetoothConnect* bluetoothConnect ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE touch threshold characteristics callback
        class PersonificationCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                PersonificationCharacteristicsCallbacks(BluetoothConnect* bluetoothConnect) : bluetoothConnect(bluetoothConnect) {};
            private:
                BluetoothConnect* bluetoothConnect ;
                void onWrite(BLECharacteristic *characteristic);
        };

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