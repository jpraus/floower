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
        void enableWifi();
        void disableWifi();
        bool isWifiEnabled();
        void updateBatteryData(uint8_t level, bool charging);

    private:
        BluetoothConnect *bluetoothConnect;
        WifiConnect *wifiConnect;
        CommandProtocol *cmdInterpreter;
        RemoteControlCallback remoteControlCallback;

        void fireRemoteControl();
};