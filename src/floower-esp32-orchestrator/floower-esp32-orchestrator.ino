#include "BLEDevice.h"
//#include "Adafruit_VL53L0X.h"
#include <NeoPixelBus.h>

// The remote service we wish to connect to.
//#define FLOOWER_SERVICE_UUID "28e17913-66c1-475f-a76e-86b5242f4cec" // standard
#define FLOOWER_SERVICE_UUID "28e17913-66c1-475f-a76e-86b5242f4ced" // special
#define FLOOWER_NAME_UUID "ab130585-2b27-498e-a5a5-019391317350" // string, NAME_MAX_LENGTH see config.h
#define FLOOWER_STATE_UUID "ac292c4b-8bd0-439b-9260-2d9526fff89a" // see StatePacketData
#define FLOOWER_STATE_CHANGE_UUID "11226015-0424-44d3-b854-9fc332756cbf" // see StateChangePacketData
//#define FLOOWER_COLORS_SCHEME_UUID "7b1e9cff-de97-4273-85e3-fd30bc72e128" // DEPRECATED: array of 3 bytes per pre-defined color [(R + G + B), (R + G + B), ..], COLOR_SCHEME_MAX_LENGTH see config.h
#define FLOOWER_COLORS_SCHEME_UUID "10b8879e-0ea0-4fe2-9055-a244a1eaca8b" // array of 2 bytes per stored HSB color, B is missing [(H/9 + S/7), (H/9 + S/7), ..], COLOR_SCHEME_MAX_LENGTH see config.h
#define FLOOWER_PERSONIFICATION_UUID "c380596f-10d2-47a7-95af-95835e0361c7" // see PersonificationPacketData (previously touch threshold)
//#define FLOOWER__UUID "03c6eedc-22b5-4a0e-9110-2cd0131cd528"

const uint8_t LED_PIN = 2;
const uint8_t PIR_PIN = 26;
const uint8_t PIR_PWR_PIN = 33;

const HsbColor colorRed(0.0, 1.0, 1.0);
const HsbColor colorGreen(0.3, 1.0, 1.0);
const HsbColor colorBlue(0.61, 1.0, 1.0);
const HsbColor colorYellow(0.16, 1.0, 1.0);
const HsbColor colorOrange(0.06, 1.0, 1.0);
const HsbColor colorWhite(0.0, 0.0, 1.0);
const HsbColor colorPurple(0.81, 1.0, 1.0);
const HsbColor colorPink(0.93, 1.0, 1.0);
const HsbColor colorBlack(0.0, 1.0, 0.0);

// State data

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
};

#define STATE_CHANGE_PACKET_SIZE 6
typedef union StateChangePacket {
  StateChangePacketData data;
  uint8_t bytes[STATE_CHANGE_PACKET_SIZE];
};

// Connection to clients

typedef struct FloowerClient {
  BLEClient* client;
  BLERemoteService* floowerService;
};

BLEScan* devicesScanner;
BLEAdvertisedDevice* connectToDevice = nullptr;

const uint8_t maxDevices = 3;
uint8_t connectedDevicesCount = 0;
FloowerClient* devices[maxDevices];

// Application State

uint8_t scanCount = maxDevices * 2;
const uint8_t scanTimeout = 2; // seconds

const uint8_t colorSchemeSize = 8;
HsbColor colorScheme[colorSchemeSize];
unsigned long colorsUsed = 0;

const unsigned long ACTIVE_DELAY = 10000;
const unsigned long PASSIVE_DELAY = 100;
bool floowersBloomed = false;
bool pirActive = false;
unsigned long nextFloowerChange = 0;

// BLE callbacks

class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* client) {
  }

  void onDisconnect(BLEClient* client) {
    ESP_LOGI(LOG_TAG, "Disconnected, reconnecting all");
    for (int i = 0; i < connectedDevicesCount; i++) {
      delete devices[i];
    }
    connectedDevicesCount = 0;
    scanCount = maxDevices * 2;
  }
};

class ScanCallbacks: public BLEAdvertisedDeviceCallbacks {
  // called for each advertising BLE server
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    ESP_LOGI(LOG_TAG, "BLE advertised device found: addr=%d", advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(FLOOWER_SERVICE_UUID))) {
      devicesScanner->stop();
      connectToDevice = new BLEAdvertisedDevice(advertisedDevice);
    }
  }
};

// application

void setup() {
  Serial.begin(115200);

  colorScheme[0] = colorWhite;
  colorScheme[1] = colorYellow;
  colorScheme[2] = colorOrange;
  colorScheme[3] = colorRed;
  colorScheme[4] = colorPink;
  colorScheme[5] = colorPurple;
  colorScheme[6] = colorBlue;
  colorScheme[7] = colorGreen;

  generateRandomSeed();
  nextFloowerChange = millis() + ACTIVE_DELAY;

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  pinMode(PIR_PIN, INPUT);
  pinMode(PIR_PWR_PIN, OUTPUT);
  digitalWrite(PIR_PWR_PIN, LOW); // power on
  delay(100);

  //if (!lox.begin()) {
  //  ESP_LOGE(LOG_TAG, "Failed to boot VL53L0X");
  //}

  BLEDevice::init("Floower Orchestrator");
  devicesScanner = BLEDevice::getScan();
  devicesScanner->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  devicesScanner->setInterval(1349);
  devicesScanner->setWindow(449);
  devicesScanner->setActiveScan(true);
}

void loop() {
  if (scanCount > 0) {
    --scanCount;
    ESP_LOGI(LOG_TAG, "Scanning (%d remains)...", scanCount);
    performScanAndConnect();
  }

  //int16_t range = measureRange();

  uint8_t pirState = digitalRead(PIR_PIN);
  digitalWrite(LED_PIN, pirState);
  if (!pirActive && pirState == HIGH) {
    pirActive = true;
  }

  if (nextFloowerChange < millis()) {
    if (pirActive) {
      StateChangePacket stateChange;
      for (int i = 0; i < connectedDevicesCount; i++) {
        RgbColor color = RgbColor(colorScheme[random(0, colorSchemeSize)]);
        stateChange.data = {100, color.R, color.G, color.B, 50, 3};
        BLERemoteCharacteristic* characteristic = devices[i]->floowerService->getCharacteristic(FLOOWER_STATE_CHANGE_UUID);
        if (characteristic != nullptr) {
          characteristic->writeValue(stateChange.bytes, STATE_CHANGE_PACKET_SIZE);
        }
      }
      nextFloowerChange = millis() + (floowersBloomed ? 5000 : 10000);
      floowersBloomed = true;
      pirActive = false;
    }
    else if (floowersBloomed) {
      StateChangePacket stateChange;
      for (int i = 0; i < connectedDevicesCount; i++) {
        stateChange.data = {0, 0, 0, 0, 50, 3};
        BLERemoteCharacteristic* characteristic = devices[i]->floowerService->getCharacteristic(FLOOWER_STATE_CHANGE_UUID);
        if (characteristic != nullptr) {
          characteristic->writeValue(stateChange.bytes, STATE_CHANGE_PACKET_SIZE);
        }
      }
      nextFloowerChange = millis() + 2500;
      floowersBloomed = false;
    }
  }
}
/*
int16_t measureRange() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!

  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    ESP_LOGI(LOG_TAG, "Distance to hand %d mm", measure.RangeMilliMeter);
    return measure.RangeMilliMeter;
  }
  else {
    return -1;
  }
}
*/
void performScanAndConnect() {
  devicesScanner->start(scanTimeout, false);

  // device found, try to connect
  if (connectToDevice != nullptr) {
    if (connectToFloower(connectToDevice)) {
      ESP_LOGI(LOG_TAG, "Connected to Floower %s", connectToDevice->getAddress().toString().c_str());
    } else {
      ESP_LOGE(LOG_TAG, "Failed to connect to Floower %s", connectToDevice->getAddress().toString().c_str());
    }
    connectToDevice = nullptr;
    if (connectedDevicesCount == maxDevices) {
      scanCount = 0; // no need to scan any longer
    }
  }
  else {
    ESP_LOGI(LOG_TAG, "No Floower found");
  }
}

bool connectToFloower(BLEAdvertisedDevice* device) {
  ESP_LOGI(LOG_TAG, "Connecting to Floower %s ...", device->getAddress().toString().c_str());

  BLEClient* client = new BLEClient();
  client->setClientCallbacks(new ClientCallbacks());
  client->connect(device);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* floowerService = client->getService(FLOOWER_SERVICE_UUID);
  if (floowerService == nullptr) {
    ESP_LOGE(LOG_TAG, "Failed to find service UUID=%s", FLOOWER_SERVICE_UUID);
    client->disconnect();
    return false;
  }

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLERemoteCharacteristic* characteristic = floowerService->getCharacteristic(FLOOWER_STATE_CHANGE_UUID);
  if (characteristic == nullptr) {
    ESP_LOGE(LOG_TAG, "Failed to find static characteristic UUID=%s", FLOOWER_STATE_UUID);
    client->disconnect();
    return false;
  }

  FloowerClient* floowerClient = new FloowerClient();
  floowerClient->client = client;
  floowerClient->floowerService = floowerService;

  devices[connectedDevicesCount] = floowerClient;
  ++connectedDevicesCount;

  /*if (characteristic->canRead()) {
    std::string value = characteristic->readValue();
    //Serial.print("The characteristic value was: ");
    //Serial.println(value.c_str());
  }
  
  if (characteristic->canNotify()) {
    characteristic->registerForNotify(notifyCallback);
  }*/

  return true;
}

RgbColor nextRandomColor() {
  if (colorsUsed > 0) {
    unsigned long maxColors = pow(2, colorSchemeSize) - 1;
    if (maxColors == colorsUsed) {
      colorsUsed = 0; // all colors used, reset
    }
  }

  uint8_t colorIndex;
  long colorCode;
  int maxIterations = colorSchemeSize * 3;

  do {
    colorIndex = random(0, colorSchemeSize);
    colorCode = 1 << colorIndex;
    maxIterations--;
  } while ((colorsUsed & colorCode) > 0 && maxIterations > 0); // already used before all the rest colors

  colorsUsed += colorCode;
  return RgbColor(colorScheme[colorIndex]);
}

void generateRandomSeed() {
    uint32_t seed;

    // random works best with a seed that can use 31 bits
    // analogRead on a unconnected pin tends toward less than four bits
    seed = analogRead(0);
    delay(1);
    for (int shifts = 3; shifts < 31; shifts += 3) {
        seed ^= analogRead(0) << shifts;
        delay(1);
    }
    ESP_LOGI(LOG_TAG, "Random seed=%d", seed);
    randomSeed(seed);
}
