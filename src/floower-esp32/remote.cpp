#include "remote.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Remote";
#endif

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

// Floower custom service
#define FLOOWER_SERVICE_UUID "28e17913-66c1-475f-a76e-86b5242f4cec"
#define FLOOWER_NAME_UUID "ab130585-2b27-498e-a5a5-019391317350" // string, NAME_MAX_LENGTH see config.h
#define FLOOWER_STATE_UUID "ac292c4b-8bd0-439b-9260-2d9526fff89a" // see StatePacketData
#define FLOOWER_STATE_CHANGE_UUID "11226015-0424-44d3-b854-9fc332756cbf" // see StateChangePacketData
//#define FLOOWER_COLORS_SCHEME_UUID "7b1e9cff-de97-4273-85e3-fd30bc72e128" // DEPRECATED: array of 3 bytes per pre-defined color [(R + G + B), (R + G + B), ..], COLOR_SCHEME_MAX_LENGTH see config.h
#define FLOOWER_COLORS_SCHEME_UUID "10b8879e-0ea0-4fe2-9055-a244a1eaca8b" // array of 2 bytes per stored HSB color, B is missing [(H/9 + S/7), (H/9 + S/7), ..], COLOR_SCHEME_MAX_LENGTH see config.h
#define FLOOWER_PERSONIFICATION_UUID "c380596f-10d2-47a7-95af-95835e0361c7" // see PersonificationPacketData (previously touch threshold)
//#define FLOOWER__UUID "03c6eedc-22b5-4a0e-9110-2cd0131cd528"

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

// BLE data packets
#define PERSONIFICATION_PACKET_SIZE 5
typedef union PersonificationPacket {
  Personification data; // see config.h
  uint8_t bytes[PERSONIFICATION_PACKET_SIZE];
};

typedef struct StatePacketData {
  uint8_t petalsOpenLevel; // normally petals open level 0-100%, read-write
  uint8_t R; // 0-255, read-write
  uint8_t G; // 0-255, read-write
  uint8_t B; // 0-255, read-write
};

#define STATE_PACKET_SIZE 4
typedef union StatePacket {
  StatePacketData data;
  uint8_t bytes[STATE_PACKET_SIZE];
};

#define STATE_TRANSITION_MODE_BIT_COLOR 0
#define STATE_TRANSITION_MODE_BIT_PETALS 1 // when this bit is set, the VALUE parameter means open level of petals (0-100%)
#define STATE_TRANSITION_MODE_BIT_ANIMATION 2 // when this bit is set, the VALUE parameter means ID of animation

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
};

#define STATE_CHANGE_PACKET_SIZE 6
typedef union StateChangePacket {
  StateChangePacketData data;
  uint8_t bytes[STATE_CHANGE_PACKET_SIZE];
};

Remote::Remote(Floower *floower, Config *config)
    : floower(floower), config(config) {
}

void Remote::init() {
  if (!initialized) {
    ESP_LOGI(LOG_TAG, "Initializing BLE server");
    BLECharacteristic* characteristic;

    // Create the BLE Device
    BLEDevice::init(config->name.c_str());

    // Create the BLE Server
    server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks(this));
  
    // Device Information profile service
    BLEService *deviceInformationService = server->createService(DEVICE_INFORMATION_UUID);
    createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_MODEL_NUMBER_STRING_UUID, "Floower");
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
  
    // Floower customer service
    floowerService = server->createService(FLOOWER_SERVICE_UUID);
  
    // device name characteristics
    characteristic = floowerService->createCharacteristic(FLOOWER_NAME_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    characteristic->setValue(config->name.c_str());
    characteristic->setCallbacks(new NameCharacteristicsCallbacks(this));
  
    // personification characteristics
    characteristic = floowerService->createCharacteristic(FLOOWER_PERSONIFICATION_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    PersonificationPacket personificationPacket = {{config->personification}};
    characteristic->setValue(personificationPacket.bytes, PERSONIFICATION_PACKET_SIZE);
    characteristic->setCallbacks(new PersonificationCharacteristicsCallbacks(this));
  
    // state read characteristics
    characteristic = floowerService->createCharacteristic(FLOOWER_STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY); // read
    RgbColor color = RgbColor(floower->getColor());
    StatePacket statePacket = {{floower->getPetalsOpenLevel(), color.R, color.G, color.B}};
    characteristic->setValue(statePacket.bytes, STATE_PACKET_SIZE);
    characteristic->addDescriptor(new BLE2902());

    // state write/change characteristics
    characteristic = floowerService->createCharacteristic(FLOOWER_STATE_CHANGE_UUID, BLECharacteristic::PROPERTY_WRITE); // write
    characteristic->setCallbacks(new StateChangeCharacteristicsCallbacks(this));
  
    // color scheme characteristics
    characteristic = floowerService->createCharacteristic(FLOOWER_COLORS_SCHEME_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    size_t size = config->colorSchemeSize * 2;
    uint8_t bytes[size];
    for (uint8_t b = 0, i = 0; b < size; b += 2, i++) {
      uint16_t valueHS = Config::encodeHSColor(config->colorScheme[i].H, config->colorScheme[i].S);
      bytes[b] = (valueHS >> 8) & 0xFF;
      bytes[b + 1] = valueHS & 0xFF;
    }
    characteristic->setValue(bytes, size);
    characteristic->setCallbacks(new ColorsSchemeCharacteristicsCallbacks(this));
  
    floowerService->start();
  
    // listen to floower state change
    floower->onChange([=](uint8_t petalsOpenLevel, HsbColor hsbColor) {
      RgbColor color = RgbColor(hsbColor);
      ESP_LOGD(LOG_TAG, "state: %d%%, [%d,%d,%d]", petalsOpenLevel, color.R, color.G, color.B);
      StatePacket statePacket = {{petalsOpenLevel, color.R, color.G, color.B}};
      BLECharacteristic* stateCharacteristic = this->floowerService->getCharacteristic(FLOOWER_STATE_UUID);
      stateCharacteristic->setValue(statePacket.bytes, STATE_PACKET_SIZE);
      stateCharacteristic->notify();
    });

    initialized = true;
  }
}

void Remote::startAdvertising() {
  if (initialized) {
    ESP_LOGI(LOG_TAG, "Start advertising ...");
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(FLOOWER_SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    bleAdvertising->setMinPreferred(0x12);
    bleAdvertising->start();
    advertising = true;
  }
}

void Remote::stopAdvertising() {
  if (initialized && !deviceConnected && advertising) {
    ESP_LOGI(LOG_TAG, "Stop advertising");
    BLEDevice::getAdvertising()->stop();
    advertising = false;
  }
}

bool Remote::isConnected() {
  return deviceConnected;
}

void Remote::onTakeOver(RemoteTakeOverCallback callback) {
  takeOverCallback = callback;
}

void Remote::setBatteryLevel(uint8_t level, bool charging) {
  if (deviceConnected && batteryService != nullptr && !floower->arePetalsMoving()) {
    ESP_LOGD(LOG_TAG, "level: %d, charging: %d", level, charging);

    BLECharacteristic* characteristic = batteryService->getCharacteristic(BATTERY_LEVEL_UUID);
    characteristic->setValue(&level, 1);
    characteristic->notify();

    uint8_t batteryState = charging ? BATTERY_POWER_STATE_CHARGING : BATTERY_POWER_STATE_DISCHARGING;
    characteristic = batteryService->getCharacteristic(BATTERY_POWER_STATE_UUID);
    characteristic->setValue(&batteryState, 1);
    characteristic->notify();
  }
}

BLECharacteristic* Remote::createROCharacteristics(BLEService *service, const char *uuid, const char *value) {
  BLECharacteristic* characteristic = service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ);                      
  characteristic->setValue(value);
}

void Remote::StateChangeCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
  std::string bytes = characteristic->getValue();
  if (bytes.length() == STATE_CHANGE_PACKET_SIZE) {
    StateChangePacket statePacket;
    for (int i = 0; i < STATE_CHANGE_PACKET_SIZE; i ++) {
      statePacket.bytes[i] = bytes[i]; 
    }

    if (CHECK_BIT(statePacket.data.mode, STATE_TRANSITION_MODE_BIT_COLOR)) {
      // blossom color
      HsbColor color = HsbColor(statePacket.data.getColor());
      remote->floower->transitionColor(color.H, color.S, remote->config->colorBrightness, statePacket.data.duration * 100);
    }
    if (CHECK_BIT(statePacket.data.mode, STATE_TRANSITION_MODE_BIT_PETALS)) {
      // petals open/close
      remote->floower->setPetalsOpenLevel(statePacket.data.value, statePacket.data.duration * 100);
    }
    else if (CHECK_BIT(statePacket.data.mode, STATE_TRANSITION_MODE_BIT_ANIMATION)) {
      // play animation (according to value)
      switch (statePacket.data.value) {
        case 1:
          remote->floower->startAnimation(FloowerColorAnimation::RAINBOW);
          break;
        case 2:
          remote->floower->startAnimation(FloowerColorAnimation::CANDLE);
          break;
      }
    }

    if (remote->takeOverCallback != nullptr) {
      remote->takeOverCallback();
    }
  }
}

void Remote::ColorsSchemeCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
  std::string bytes = characteristic->getValue();
  if (bytes.length() > 0) {
    size_t size = floor(bytes.length() / 2);
    size_t bytesSize = size * 2;
    HsbColor colors[size];

    ESP_LOGI(LOG_TAG, "New color scheme: %d", size);
    for (uint8_t b = 0, i = 0; b < bytesSize; b += 2, i++) {
      colors[i] = Config::decodeHSColor(((uint16_t)bytes[b] << 8) | bytes[b + 1]);
      ESP_LOGI(LOG_TAG, "Color %d: %.2f,%.2f", i, colors[i].H, colors[i].S);
    }

    remote->config->setColorScheme(colors, size);
    remote->config->commit();
  }
}

void Remote::NameCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
  std::string bytes = characteristic->getValue();
  if (bytes.length() > 0) {
    size_t length = min(bytes.length(), (unsigned int) NAME_MAX_LENGTH) + 1;
    char data[length];
    for (int i = 0; i < length; i ++) {
      data[i] = bytes[i]; 
    }
    data[length] = '\0';

    ESP_LOGI(LOG_TAG, "New name: %s", data);
    remote->config->setName(String(data));
    remote->config->commit();
  }
}

void Remote::PersonificationCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
  std::string bytes = characteristic->getValue();
  if (bytes.length() == PERSONIFICATION_PACKET_SIZE) {
    PersonificationPacket personificationPacket;
    for (int i = 0; i < PERSONIFICATION_PACKET_SIZE; i ++) {
      personificationPacket.bytes[i] = bytes[i]; 
    }

    if (personificationPacket.data.speed < 5) {
      personificationPacket.data.speed = 5;
    }
    if (personificationPacket.data.maxOpenLevel > 100) {
      personificationPacket.data.maxOpenLevel = 100;
    }
    if (personificationPacket.data.colorBrightness > 100) {
      personificationPacket.data.colorBrightness = 100;
    }
    ESP_LOGI(LOG_TAG, "New personification: touchThreshold=%d, behavior=%d, speed=%d, maxOpenLevel=%d, colorBrightness=%d", personificationPacket.data.touchThreshold, personificationPacket.data.behavior, personificationPacket.data.speed, personificationPacket.data.maxOpenLevel, personificationPacket.data.colorBrightness);
    
    remote->config->setPersonification(personificationPacket.data);
    remote->config->commit();
    remote->floower->enableTouch();
  }
}

void Remote::ServerCallbacks::onConnect(BLEServer* server) {
  ESP_LOGI(LOG_TAG, "Connected to client");
  remote->deviceConnected = true;
  remote->advertising = false;

  if (!remote->config->initRemoteOnStartup) {
    remote->config->setRemoteOnStartup(true);
    remote->config->commit();
  }
};

void Remote::ServerCallbacks::onDisconnect(BLEServer* server) {
  ESP_LOGI(LOG_TAG, "Disconnected, start advertising");
  remote->deviceConnected = false;
  server->startAdvertising();
  remote->advertising = true;
};
