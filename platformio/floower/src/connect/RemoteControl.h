#pragma once

#include "Arduino.h"
#include "Config.h"
#include "hardware/Floower.h"
#include "BluetoothConnect.h"
#include "CommandInterpreter.h"
#include "WifiConnect.h"

typedef std::function<void()> RemoteControlCallback;

class RemoteControl {
    public:
        RemoteControl(BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect, CommandInterpreter *cmdInterpreter);

        void onRemoteControl(RemoteControlCallback callback);
        void enableBluetooth();
        void disableBluetooth();
        bool isBluetoothConnected();
        void enableWifi();
        void disableWifi();
        bool isWifiEnabled();
        void setBatteryLevel(uint8_t level, bool charging);

    private:
        BluetoothConnect *bluetoothConnect;
        WifiConnect *wifiConnect;
        CommandInterpreter *cmdInterpreter;
        RemoteControlCallback remoteControlCallback;

        void fireRemoteControl();
};