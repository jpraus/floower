#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "BluetoothConnect.h"
#include "CommandProtocol.h"
#include "WifiConnect.h"

typedef std::function<void()> RemoteControlCallback;

class RemoteControl {
    public:
        RemoteControl(BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect, CommandProtocol *cmdInterpreter);

        void onRemoteControl(RemoteControlCallback callback);
        void enableBluetooth();
        void disableBluetooth();
        bool isBluetoothConnected();
        bool isWifiConnected();
        void enableWifi();
        void disableWifi();
        bool isWifiEnabled();
        void updateStatusData(uint8_t batteryLevel, bool batteryCharging);

    private:
        BluetoothConnect *bluetoothConnect;
        WifiConnect *wifiConnect;
        CommandProtocol *cmdInterpreter;
        RemoteControlCallback remoteControlCallback;

        void fireRemoteControl();
};