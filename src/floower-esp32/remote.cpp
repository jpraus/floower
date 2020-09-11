#include "remote.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

// Floower custom service
#define FLOOWER_SERVICE_UUID "28e17913-66c1-475f-a76e-86b5242f4cec"
#define FLOOWER_NAME_UUID "ab130585-2b27-498e-a5a5-019391317350" // string
#define FLOOWER_STATE_UUID "ac292c4b-8bd0-439b-9260-2d9526fff89a" // 4 bytes (open level + R + G + B)
#define FLOOWER_COLOR_RGB_UUID "151a039e-68ee-4009-853d-cd9d271e4a6e" // 3 bytes (R + G + B)
#define FLOOWER_COLORS_SCHEME_UUID "7b1e9cff-de97-4273-85e3-fd30bc72e128" // array of 3 bytes per pre-defined color [(R + G + B), (R +G + B), ..]
//#define FLOOWER__UUID "ac292c4b-8bd0-439b-9260-2d9526fff89a"
//#define FLOOWER__UUID "11226015-0424-44d3-b854-9fc332756cbf"
//#define FLOOWER__UUID "c380596f-10d2-47a7-95af-95835e0361c7"
//#define FLOOWER__UUID "10b8879e-0ea0-4fe2-9055-a244a1eaca8b"
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

Remote::Remote(Floower *floower)
    : floower(floower) {
}

void Remote::init() {
  Serial.println("Initializing BLE server");

  // Create the BLE Device
  BLEDevice::init("Floower");

  // Create the BLE Server
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks(this));

  // Start the service
  Serial.println("Starting BLE services");

  // Device Information profile service
  BLEService *deviceInformationService = server->createService(DEVICE_INFORMATION_UUID);
  createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_MODEL_NUMBER_STRING_UUID, "Floower");
  createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_SERIAL_NUMBER_UUID, "0016");
  createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_FIRMWARE_REVISION_UUID, "1.0");
  createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_HARDWARE_REVISION_UUID, "6");
  createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_MANUFACTURER_NAME_UUID, "Floower Lab s.r.o.");
  deviceInformationService->start();

  // Battery level profile service
  BLEService *batteryService = server->createService(BATTERY_UUID);
  createROCharacteristics(batteryService, BATTERY_LEVEL_UUID, 65);
  batteryService->start();

  // Floower customer service
  BLEService *floowerService = server->createService(FLOOWER_SERVICE_UUID);  
  createROCharacteristics(deviceInformationService, DEVICE_INFORMATION_MODEL_NUMBER_STRING_UUID, "Floower");

  String name = "Floower";
  BLECharacteristic* nameCharacteristic = floowerService->createCharacteristic(FLOOWER_NAME_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  //nameCharacteristic->setCallbacks(new ColorRgbCharacteristicsCallbacks());
  nameCharacteristic->setValue("Floower");

  uint8_t color[3] = {69, 100, 255}; // RGB
  BLECharacteristic* colorRgbCharacteristic = floowerService->createCharacteristic(FLOOWER_COLOR_RGB_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  colorRgbCharacteristic->setCallbacks(new ColorRGBCharacteristicsCallbacks(this));
  colorRgbCharacteristic->setValue(color, 3);

  StatePacket statePacket = {{0, 69, 100, 255}};
  BLECharacteristic* stateCharacteristic = floowerService->createCharacteristic(FLOOWER_STATE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  stateCharacteristic->setCallbacks(new StateCharacteristicsCallbacks(this));
  stateCharacteristic->setValue(statePacket.bytes, PACKET_DATA_SIZE);

  floowerService->start();

  // Start advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(FLOOWER_SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  advertising->setMinPreferred(0x12);
  advertising->start();
  Serial.println("Waiting a client connection to notify...");
}

void Remote::update() {
  // TODO: check if initialized
}

BLECharacteristic* Remote::createROCharacteristics(BLEService *service, const char *uuid, const char *value) {
  BLECharacteristic* characteristic = service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ);                      
  characteristic->setValue(value);
}

BLECharacteristic* Remote::createROCharacteristics(BLEService *service, const char *uuid, int value) {
  BLECharacteristic* characteristic = service->createCharacteristic(uuid, BLECharacteristic::PROPERTY_READ);
  characteristic->setValue(value);
  return characteristic;
}

void Remote::StateCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
  Serial.println("New state");
  std::string bytes = characteristic->getValue();
  if (bytes.length() == 4) {
    StatePacket statePacket;
    statePacket.data = {bytes[0], bytes[1], bytes[2], bytes[3]};
    remote->floower->setPetalsOpenLevel(statePacket.data.petalsOpenLevel, 500); // TODO: transition time
    remote->floower->setColor(statePacket.data.getColor(), FloowerColorMode::TRANSITION, 100); // TODO: transition time
  }
}

void Remote::ColorRGBCharacteristicsCallbacks::onWrite(BLECharacteristic *characteristic) {
  Serial.println("New color");
  std::string bytes = characteristic->getValue();
  if (bytes.length() == 3) {
    RgbColor color = RgbColor(bytes[0], bytes[1], bytes[2]);
    Serial.print("Received Color: ");
    Serial.print(color.R);
    Serial.print(",");
    Serial.print(color.G);
    Serial.print(",");
    Serial.println(color.B);
    remote->floower->setColor(color, FloowerColorMode::TRANSITION, 100);
  }
}

void Remote::ServerCallbacks::onConnect(BLEServer* server) {
  Serial.println("Connected to client");
  remote->deviceConnected = true;
};

void Remote::ServerCallbacks::onDisconnect(BLEServer* server) {
  Serial.println("Disconnected, start advertising");
  remote->deviceConnected = false;
  remote->floower->setColor(RgbColor(0), FloowerColorMode::TRANSITION, 100);
  server->startAdvertising(); // restart advertising
};
