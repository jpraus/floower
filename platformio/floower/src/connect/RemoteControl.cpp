#include "RemoteControl.h"

RemoteControl::RemoteControl(BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect, CommandInterpreter *cmdInterpreter):
        bluetoothConnect(bluetoothConnect), wifiConnect(wifiConnect), cmdInterpreter(cmdInterpreter) {
    cmdInterpreter->onRemoteControl([=]() {
        if (this->remoteControlCallback != nullptr) {
            remoteControlCallback();
            Serial.println("Remote control");
        }
    });
    // deprecated
    bluetoothConnect->onRemoteControl([=]() {
        if (this->remoteControlCallback != nullptr) {
            remoteControlCallback();
            Serial.println("Remote control");
        }
    });
}

void RemoteControl::onRemoteControl(RemoteControlCallback callback) {
    remoteControlCallback = callback;
}

void RemoteControl::enableBluetooth() {
    bluetoothConnect->enable();
}

void RemoteControl::disableBluetooth() {
    bluetoothConnect->disable();
}

bool RemoteControl::isBluetoothConnected() {
    return bluetoothConnect->isConnected();
}

void RemoteControl::enableWifi() {
    wifiConnect->enable();
}

void RemoteControl::disableWifi() {
    wifiConnect->disable();
}

bool RemoteControl::isWifiEnabled() {
    return wifiConnect->isEnabled();
}

void RemoteControl::setBatteryLevel(uint8_t level, bool charging) {
    bluetoothConnect->setBatteryLevel(level, charging);
}