#include "RemoteControl.h"

RemoteControl::RemoteControl(BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect, CommandProtocol *cmdInterpreter):
        bluetoothConnect(bluetoothConnect), wifiConnect(wifiConnect), cmdInterpreter(cmdInterpreter) {
    cmdInterpreter->onControlCommand([=]() { fireRemoteControl(); });
}

void RemoteControl::onRemoteControl(RemoteControlCallback callback) {
    remoteControlCallback = callback;
}

void RemoteControl::fireRemoteControl() {
    if (remoteControlCallback != nullptr) {
        remoteControlCallback();
    }
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

void RemoteControl::updateBatteryData(uint8_t level, bool charging) {
    bluetoothConnect->updateBatteryData(level, charging);
    wifiConnect->updateBatteryData(level, charging);
}