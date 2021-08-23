#pragma once

#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "Config.h"
#include "hardware/Floower.h"

typedef struct StateChangePacketData {
    uint8_t value;
    uint8_t R; // 0-255, read-write
    uint8_t G; // 0-255, read-write
    uint8_t B; // 0-255, read-write
    uint8_t duration; // 100 of milliseconds
    uint8_t mode; // 8 flags, see defines above

    HsbColor getColor() {
        return HsbColor(RgbColor(R, G, B));
    }
} StateChangePacketData;

#define STATE_TRANSITION_MODE_BIT_COLOR 0
#define STATE_TRANSITION_MODE_BIT_PETALS 1 // when this bit is set, the VALUE parameter means open level of petals (0-100%)
#define STATE_TRANSITION_MODE_BIT_ANIMATION 2 // when this bit is set, the VALUE parameter means ID of animation

#define STATE_CHANGE_PACKET_SIZE 6
typedef union StateChangePacket {
    StateChangePacketData data;
    uint8_t bytes[STATE_CHANGE_PACKET_SIZE];
} StateChangePacket;

typedef std::function<void(StateChangePacketData data)> BluetoothControlRemoteChangeCallback;

class BluetoothControl {
    public:
        BluetoothControl(Floower *floower, Config *config);
        void init();
        void startAdvertising();
        void stopAdvertising();
        void setBatteryLevel(uint8_t level, bool charging);
        bool isConnected();
        void onRemoteChange(BluetoothControlRemoteChangeCallback callback);

    private:
        Floower *floower;
        Config *config;
        BluetoothControlRemoteChangeCallback remoteChangeCallback;
        BLEServer *server = nullptr;
        BLEService *floowerService = nullptr;
        BLEService *batteryService = nullptr;

        bool deviceConnected = false;
        bool advertising = false;
        bool initialized = false;

        BLECharacteristic* createROCharacteristics(BLEService *service, const char *uuid, const char *value);

        // BLE state characteristics callback
        class StateChangeCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                StateChangeCharacteristicsCallbacks(BluetoothControl* bluetoothControl) : bluetoothControl(bluetoothControl) {};
            private:
                BluetoothControl* bluetoothControl ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE name characteristics callback
        class NameCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                NameCharacteristicsCallbacks(BluetoothControl* bluetoothControl) : bluetoothControl(bluetoothControl) {};
            private:
                BluetoothControl* bluetoothControl ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE color scheme characteristics callback
        class ColorsSchemeCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                ColorsSchemeCharacteristicsCallbacks(BluetoothControl* bluetoothControl) : bluetoothControl(bluetoothControl) {};
            private:
                BluetoothControl* bluetoothControl ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE touch threshold characteristics callback
        class PersonificationCharacteristicsCallbacks : public BLECharacteristicCallbacks {
            public:
                PersonificationCharacteristicsCallbacks(BluetoothControl* bluetoothControl) : bluetoothControl(bluetoothControl) {};
            private:
                BluetoothControl* bluetoothControl ;
                void onWrite(BLECharacteristic *characteristic);
        };

        // BLE server callbacks impl
        class ServerCallbacks : public BLEServerCallbacks {
            public:
                ServerCallbacks(BluetoothControl* bluetoothControl) : bluetoothControl(bluetoothControl) {};
            private:
                BluetoothControl* bluetoothControl ;
                void onConnect(BLEServer* server);
                void onDisconnect(BLEServer* server);
        };
};