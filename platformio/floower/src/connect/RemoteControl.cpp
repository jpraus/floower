#include "RemoteControl.h"

RemoteControl::RemoteControl(BluetoothConnect *bluetoothConnect, WifiConnect *wifiConnect, MqttConnect *mqttConnect, CommandProtocol *cmdInterpreter):
        bluetoothConnect(bluetoothConnect), wifiConnect(wifiConnect), mqttConnect(mqttConnect), cmdInterpreter(cmdInterpreter) {
    cmdInterpreter->onControlCommand([=]() { fireRemoteControl(); });
    cmdInterpreter->onRunOTAUpdate([=](String firmwareUrl) { fireRunUpdate(firmwareUrl); });
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

bool RemoteControl::isWifiConnected() {
    return wifiConnect->isConnected();
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

void RemoteControl::enableMqtt() {
    mqttConnect->enable();
}

void RemoteControl::disableMqtt() {
    mqttConnect->disable();
}

bool RemoteControl::isMqttConnected() {
    return mqttConnect->isConnected();
}

void RemoteControl::updateStatusData(uint8_t batteryLevel, bool batteryCharging) {
    wifiConnect->updateStatusData(batteryLevel, batteryCharging);
    bluetoothConnect->updateStatusData(batteryLevel, batteryCharging, wifiConnect->getStatus());
}

void RemoteControl::runUpdate(String firmwareUrl) {
    wifiConnect->startOTAUpdate(firmwareUrl);
}

bool RemoteControl::isUpdateRunning() {
    return wifiConnect->isOTAUpdateRunning();
}

void RemoteControl::onRunUpdate(RunUpdateCallback callback) {
    runUpdateCallback = callback;
}

void RemoteControl::fireRunUpdate(String firmwareUrl) {
    if (runUpdateCallback != nullptr) {
        runUpdateCallback(firmwareUrl);
    }
}