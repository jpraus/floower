#include "BluetoothConnect.h"
#include "MD5Builder.h"
#include "WiFi.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "BluetoothControl";
#endif

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

// Floower custom services (max 6 characteristics per service)
// command protocol service
#define FLOOWER_SERVICE_COMMAND_UUID "28e17913-66c1-475f-a76e-86b5242f4cec"
#define FLOOWER_CHAR_STATE_UUID "ac292c4b-8bd0-439b-9260-2d9526fff89a" // see StatePacketData
#define FLOOWER_CHAR_COMMAND_UUID "03c6eedc-22b5-4a0e-9110-2cd0131cd528" // command interface to replace all other

// config service
#define FLOOWER_SERVICE_CONFIG_UUID "96f75832-8ce3-4800-b528-b39225282e9e"
#define FLOOWER_CHAR_NAME_UUID "ab130585-2b27-498e-a5a5-019391317350" // string, NAME_MAX_LENGTH see config.h
#define FLOOWER_CHAR_SPEED_TENTS_OF_SEC "a54d8bbb-379b-425e-8d6e-84d0b20309aa" // default speed of open/close cycle in tents of second
#define FLOOWER_CHAR_COLOR_BRIGHTNESS "2be47e11-088c-47aa-ae77-e2453d840833" // 0-100 color brightness level
#define FLOOWER_CHAR_MAX_OPEN_LEVEL "86bbd86c-6056-446e-a63b-ebb313bb65a5" // 0-100 max open level limit
#define FLOOWER_COLORS_SCHEME_UUID "10b8879e-0ea0-4fe2-9055-a244a1eaca8b" // array of 2 bytes per stored HSB color, B is missing [(H/9 + S/7), (H/9 + S/7), ..], COLOR_SCHEME_MAX_LENGTH see config.h

// connect service
#define FLOOWER_SERVICE_CONNECT_UUID "c1724350-5989-4741-a21b-5878621468fa"
#define FLOOWER_CHAR_WIFI_SSID "31c43b5e-6aed-47b7-bf10-0c7bbd563024" // string of wifi ssid
#define FLOOWER_CHAR_FLOUD_DEVICE_ID "297a6db3-168a-42a6-9fbe-29e5c6e56f16" // string of floud device id
#define FLOOWER_CHAR_FLOUD_TOKEN_HASH "8ba180d5-c5af-4240-a1e8-9314b1470874" // string of floud token hash
#define FLOOWER_CHAR_WIFI_STATUS "bf970815-44d5-416b-a737-0bf74195d4b5" // status code of wifi

// https://docs.springcard.com/books/SpringCore/Host_interfaces/Physical_and_Transport/Bluetooth/Standard_Services
// Device Information profile
#define DEVICE_INFORMATION_UUID "180A"
#define DEVICE_INFORMATION_MODEL_NUMBER_STRING_UUID "2A24" // string
#define DEVICE_INFORMATION_SERIAL_NUMBER_UUID "2A25" // string
#define DEVICE_INFORMATION_FIRMWARE_REVISION_UUID "2A26" // string M.mm.bbbbb
#define DEVICE_INFORMATION_HARDWARE_REVISION_UUID "2A27" // string
#define DEVICE_INFORMATION_SOFTWARE_REVISION_UUID "2A28" // string M.mm.bbbbb
#define DEVICE_INFORMATION_MANUFACTURER_NAME_UUID "2A29" // string

// Battery level profile
#define BATTERY_UUID "180F"
#define BATTERY_LEVEL_UUID "2A19" // uint8
#define BATTERY_POWER_STATE_UUID "2A1A" // uint8 of states

#define BATTERY_POWER_STATE_CHARGING B00111011
#define BATTERY_POWER_STATE_DISCHARGING B00101111

typedef struct StateData {
    int8_t petalsOpenLevel; // normally petals open level 0-100%, read-write
    uint8_t R; // 0-255, read-write
    uint8_t G; // 0-255, read-write
    uint8_t B; // 0-255, read-write
} StateData;

BluetoothConnect::BluetoothConnect(Floower *floower, Config *config, CommandProtocol *cmdProtocol)
    : floower(floower), config(config), cmdProtocol(cmdProtocol) {
}

void BluetoothConnect::enable() {
    enabled = true;
    if (!initialized) {
        init();
    }
    startAdvertising();
}

void BluetoothConnect::disable() {
    enabled = false;
    if (advertising) {
        stopAdvertising();
    }
    server->disconnect(connectionId);
}

void BluetoothConnect::init() {
    ESP_LOGI(LOG_TAG, "Initializing BLE server");
    BLECharacteristic* characteristic;

    // Create the BLE Device
    BLEDevice::init(config->name.c_str());

    // Create the BLE Server
    server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks(this));
    
    // Device Information profile service
    BLEService *deviceInformationService = server->createService(DEVICE_INFORMATION_UUID);
    createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_MODEL_NUMBER_STRING_UUID, String(config->modelName).c_str());
    createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_SERIAL_NUMBER_UUID, String(config->serialNumber).c_str());
    createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_FIRMWARE_REVISION_UUID, String(config->firmwareVersion).c_str());
    createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_HARDWARE_REVISION_UUID, String(config->hardwareRevision).c_str());
    createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_MANUFACTURER_NAME_UUID, "Floower Lab s.r.o.");
    deviceInformationService->start();
    
    // Battery level profile service
    batteryService = server->createService(BATTERY_UUID);
    characteristic = batteryService->createCharacteristic(BATTERY_LEVEL_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    characteristic->addDescriptor(new BLE2902());
    characteristic = batteryService->createCharacteristic(BATTERY_POWER_STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    characteristic->addDescriptor(new BLE2902());
    batteryService->start();
    
    // command protocol service
    commandService = server->createService(FLOOWER_SERVICE_COMMAND_UUID);
    characteristic = commandService->createCharacteristic(FLOOWER_CHAR_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
    characteristic->setCallbacks(new CommandCharacteristicsCallbacks(this));
    characteristic = commandService->createCharacteristic(FLOOWER_CHAR_STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY); // read
    RgbColor color = RgbColor(floower->getColor());
    StateData stateData = {floower->getPetalsOpenLevel(), color.R, color.G, color.B};
    characteristic->setValue((uint8_t *) &stateData, sizeof(stateData));
    characteristic->addDescriptor(new BLE2902());
    commandService->start();
    
    // config service
    configService = server->createService(FLOOWER_SERVICE_CONFIG_UUID);
    configService->createCharacteristic(FLOOWER_CHAR_NAME_UUID, BLECharacteristic::PROPERTY_READ);
    configService->createCharacteristic(FLOOWER_CHAR_MAX_OPEN_LEVEL, BLECharacteristic::PROPERTY_READ);
    configService->createCharacteristic(FLOOWER_CHAR_COLOR_BRIGHTNESS, BLECharacteristic::PROPERTY_READ);
    configService->createCharacteristic(FLOOWER_CHAR_SPEED_TENTS_OF_SEC, BLECharacteristic::PROPERTY_READ);
    configService->createCharacteristic(FLOOWER_COLORS_SCHEME_UUID, BLECharacteristic::PROPERTY_READ);
    configService->start();

    // connect service
    connectService = server->createService(FLOOWER_SERVICE_CONNECT_UUID);
    connectService->createCharacteristic(FLOOWER_CHAR_WIFI_SSID, BLECharacteristic::PROPERTY_READ);
    connectService->createCharacteristic(FLOOWER_CHAR_FLOUD_DEVICE_ID, BLECharacteristic::PROPERTY_READ);
    connectService->createCharacteristic(FLOOWER_CHAR_FLOUD_TOKEN_HASH, BLECharacteristic::PROPERTY_READ);
    characteristic = connectService->createCharacteristic(FLOOWER_CHAR_WIFI_STATUS, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    characteristic->addDescriptor(new BLE2902());
    connectService->start();

    // set values for config and connect service
    reloadConfig();

    initialized = true;
}

void BluetoothConnect::reloadConfig() {
    BLECharacteristic* characteristic;

    // connect service
    if (connectService != nullptr) {
        characteristic = connectService->getCharacteristic(FLOOWER_CHAR_WIFI_SSID);
        characteristic->setValue(String(config->wifiSsid).c_str());
        characteristic = connectService->getCharacteristic(FLOOWER_CHAR_FLOUD_DEVICE_ID);
        characteristic->setValue(String(config->floudDeviceId).c_str());
        characteristic = connectService->getCharacteristic(FLOOWER_CHAR_FLOUD_TOKEN_HASH);
        characteristic->setValue(md5(config->floudToken).c_str());
    }

    // config service
    if (configService != nullptr) {
        characteristic = configService->getCharacteristic(FLOOWER_CHAR_NAME_UUID);
        characteristic->setValue(String(config->name).c_str());
        characteristic = configService->getCharacteristic(FLOOWER_CHAR_MAX_OPEN_LEVEL);
        characteristic->setValue(&config->maxOpenLevel, 1);
        characteristic = configService->getCharacteristic(FLOOWER_CHAR_COLOR_BRIGHTNESS);
        characteristic->setValue(&config->colorBrightness, 1);
        characteristic = configService->getCharacteristic(FLOOWER_CHAR_SPEED_TENTS_OF_SEC);
        characteristic->setValue(&config->speed, 1);
        characteristic = configService->getCharacteristic(FLOOWER_COLORS_SCHEME_UUID);
        size_t size = config->colorSchemeSize * 2;
        uint8_t bytes[size];
        for (uint8_t b = 0, i = 0; b < size; b += 2, i++) {
            uint16_t valueHS = Config::encodeHSColor(config->colorScheme[i].H, config->colorScheme[i].S);
            bytes[b] = (valueHS >> 8) & 0xFF;
            bytes[b + 1] = valueHS & 0xFF;
        }
        characteristic->setValue(bytes, size);
    }
}

void BluetoothConnect::startAdvertising() {
    ESP_LOGI(LOG_TAG, "Start advertising ...");
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(FLOOWER_SERVICE_COMMAND_UUID);
    bleAdvertising->addServiceUUID(FLOOWER_SERVICE_CONFIG_UUID);
    bleAdvertising->addServiceUUID(FLOOWER_SERVICE_CONNECT_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    bleAdvertising->setMinPreferred(0x12);
    bleAdvertising->start();
    advertising = true;
}

void BluetoothConnect::stopAdvertising() {
    ESP_LOGI(LOG_TAG, "Stop advertising");
    BLEDevice::getAdvertising()->stop();
    advertising = false;
}

bool BluetoothConnect::isConnected() {
    return deviceConnected;
}

String BluetoothConnect::md5(String value) {
    MD5Builder md5;
    md5.begin();
    md5.add(String(value));
    md5.calculate();
    return md5.toString();
}

void BluetoothConnect::updateStatusData(uint8_t batteryLevel, bool batteryCharging, uint8_t wifiStatus) {
    ESP_LOGD(LOG_TAG, "level: %d, charging: %d, wifi status: %d", batteryLevel, batteryCharging, wifiStatus);
    if (deviceConnected && batteryService != nullptr) {
        BLECharacteristic* characteristic = batteryService->getCharacteristic(BATTERY_LEVEL_UUID);
        characteristic->setValue(&batteryLevel, 1);
        characteristic->notify();

        uint8_t batteryState = batteryCharging ? BATTERY_POWER_STATE_CHARGING : BATTERY_POWER_STATE_DISCHARGING;
        characteristic = batteryService->getCharacteristic(BATTERY_POWER_STATE_UUID);
        characteristic->setValue(&batteryState, 1);
        characteristic->notify();
    }
    if (deviceConnected && connectService != nullptr) {
        BLECharacteristic* characteristic = connectService->getCharacteristic(FLOOWER_CHAR_WIFI_STATUS);
        characteristic->setValue(&wifiStatus, 1);
        characteristic->notify();
    }
}

void BluetoothConnect::updateFloowerState(int8_t petalsOpenLevel, HsbColor hsbColor) {
    if (commandService != nullptr) {
        RgbColor color = RgbColor(hsbColor);
        ESP_LOGD(LOG_TAG, "state: %d%%, [%d,%d,%d]", petalsOpenLevel, color.R, color.G, color.B);
        StateData stateData = {petalsOpenLevel, color.R, color.G, color.B};
        BLECharacteristic* stateCharacteristic = this->commandService->getCharacteristic(FLOOWER_CHAR_STATE_UUID);
        stateCharacteristic->setValue((uint8_t *) &stateData, sizeof(stateData));
        stateCharacteristic->notify();
    }
}

BLECharacteristic* BluetoothConnect::createROCharacteristics(BLEService *service, const char *uuid, const char *value) {
    BLECharacteristic* characteristic = service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ);                      
    characteristic->setValue(value);
    return characteristic;
}

void BluetoothConnect::CommandCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
    std::string bytes = characteristic->getValue();
    ESP_LOGI(LOG_TAG, "Command received: %", bytes);

    size_t headerSize = sizeof(CommandMessageHeader);
    if (bytes.length() >= headerSize) {
        CommandMessageHeader messageHeader;
        memcpy(&messageHeader, bytes.data(), headerSize);
        messageHeader.type = ntohs(messageHeader.type);
        messageHeader.id = ntohs(messageHeader.id);
        messageHeader.length = ntohs(messageHeader.length);

        ESP_LOGI(LOG_TAG, "Got message: %d/%d/%d", messageHeader.type, messageHeader.id, messageHeader.length);

        // validate payload
        if (messageHeader.length > 0) {
            size_t available = bytes.length() - headerSize;
            if (available > MAX_MESSAGE_PAYLOAD_BYTES || available < messageHeader.length) {
                return;
            }
        }

        bluetoothConnect->cmdProtocol->run(messageHeader.type, bytes.data() + headerSize, messageHeader.length);
    }
}

void BluetoothConnect::ServerCallbacks::onConnect(BLEServer* server) {
    ESP_LOGI(LOG_TAG, "Connected to client");
    bluetoothConnect->deviceConnected = true;
    bluetoothConnect->advertising = false;
    bluetoothConnect->connectionId = server->getConnId(); // first one is 0

    if (!bluetoothConnect->config->bluetoothAlwaysOn) {
        bluetoothConnect->config->setBluetoothAlwaysOn(true);
        bluetoothConnect->config->commit();
    }
};

void BluetoothConnect::ServerCallbacks::onDisconnect(BLEServer* server) {
    ESP_LOGI(LOG_TAG, "Disconnected, start advertising");
    bluetoothConnect->deviceConnected = false;
    if (bluetoothConnect->enabled) {
        server->startAdvertising();
        bluetoothConnect->advertising = true;
    }
};
