#include "config.h"
#include "math.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Config";
#endif

// max 512B of EEPROM

#define EEPROM_SIZE 128
#define CONFIG_VERSION 2

// DO NOT CHANGE MEMORY ADDRESSES!
// Hardware constants (reserved 0-19)
#define EEPROM_ADDRESS_CONFIG_VERSION 0 // byte - version of configuration
#define EEPROM_ADDRESS_LEDS_MODEL 1 // 0 - WS2812b, 1 - SK6812 (1 byte) NOT USED ANYMORE (since version 1)
#define EEPROM_ADDRESS_SERVO_CLOSED 2 // integer (2 bytes) calibrated position of servo blossom closed (since version 1)
#define EEPROM_ADDRESS_SERVO_OPEN 4 // integer (2 bytes) calibrated position of servo blossom open (since version 1)
#define EEPROM_ADDRESS_REVISION 6 // byte - revision number of the logic board to enable features (since version 2)
#define EEPROM_ADDRESS_SERIALNUMBER 7 // integer (2 bytes) (since version 2)

// Customizable values (20+)
#define EEPROM_ADDRESS_TOUCH_TRESHOLD 20 // byte (since version 2)
#define EEPROM_ADDRESS_BEHAVIOR 21 // byte - enumeration of predefined behaviors (since version 2)
#define EEPROM_ADDRESS_COLOR_SCHEME_LENGTH 22 // 1 byte - number of colors set in EEPROM_ADDRESS_COLOR_SCHEME (since version 2)
#define EEPROM_ADDRESS_NAME_LENGTH 23 // 1 byte - lenght of data stored in EEPROM_ADDRESS_NAME (since version 2)
#define EEPROM_ADDRESS_INIT_REMOTE_ON_STARTUP 24 // 1 byte - 0 = do not init remote on startups, 1 = init remote on startup
#define EEPROM_ADDRESS_COLOR_SCHEME 30 // (30-59) 30 bytes (10x RGB set) - list of up to 10 colors (since version 2)
#define EEPROM_ADDRESS_NAME 60 // (60-99) max 25 (40 reserved) chars (since version 2)

void Config::begin() {
  EEPROM.begin(EEPROM_SIZE);
}

void Config::load() {
  uint8_t configVersion = EEPROM.read(EEPROM_ADDRESS_CONFIG_VERSION);

  if (configVersion > 0) {
    servoClosed = readInt(EEPROM_ADDRESS_SERVO_CLOSED);
    servoOpen = readInt(EEPROM_ADDRESS_SERVO_OPEN);
  }

  // new version of config
  if (configVersion < CONFIG_VERSION) {
    // for backward compatibility, reset to factory settings
    ESP_LOGW(LOG_TAG, "Config outdated %d", configVersion);
    hardwareCalibration(servoClosed, servoOpen, 0, 0);
    factorySettings();
    commit();
  }
  else {
    hardwareRevision = EEPROM.read(EEPROM_ADDRESS_REVISION);
    serialNumber = readInt(EEPROM_ADDRESS_SERIALNUMBER);
    touchTreshold = EEPROM.read(EEPROM_ADDRESS_TOUCH_TRESHOLD);
    behavior = EEPROM.read(EEPROM_ADDRESS_BEHAVIOR);
    readColorScheme();
    readName();
    initRemoteOnStartup = (EEPROM.read(EEPROM_ADDRESS_INIT_REMOTE_ON_STARTUP) == 1);
  }

  ESP_LOGI(LOG_TAG, "Config ready");
  ESP_LOGI(LOG_TAG, "HW: %d -> %d, R%d, SN%d", servoClosed, servoOpen, hardwareRevision, serialNumber);
  ESP_LOGI(LOG_TAG, "SW: tt%d, bhvr%d, r%d, %s", touchTreshold, behavior, initRemoteOnStartup, name.c_str());
  for (uint8_t i = 0; i < colorSchemeSize; i++) {
    ESP_LOGI(LOG_TAG, "Color %d: %d,%d,%d", i, colorScheme[i].R, colorScheme[i].G, colorScheme[i].B);
  }
}

void Config::hardwareCalibration(unsigned int servoClosed, unsigned int servoOpen, uint8_t hardwareRevision, unsigned int serialNumber) {
  ESP_LOGW(LOG_TAG, "New HW config: %d -> %d, R%d, SN%d", servoClosed, servoOpen, hardwareRevision, serialNumber);
  EEPROM.write(EEPROM_ADDRESS_CONFIG_VERSION, CONFIG_VERSION);
  EEPROM.write(EEPROM_ADDRESS_LEDS_MODEL, 0); // 0 - WS2812b, 1 - SK6812 not used any longer
  writeInt(EEPROM_ADDRESS_SERVO_CLOSED, servoClosed);
  writeInt(EEPROM_ADDRESS_SERVO_OPEN, servoOpen);
  EEPROM.write(EEPROM_ADDRESS_REVISION, hardwareRevision);
  writeInt(EEPROM_ADDRESS_SERIALNUMBER, serialNumber);

  this->servoClosed = servoClosed;
  this->servoOpen = servoOpen;
  this->hardwareRevision = hardwareRevision;
  this->serialNumber = serialNumber;
}

void Config::factorySettings() {
  ESP_LOGI(LOG_TAG, "Factory reset");
  setTouchTreshold(DEFAULT_TOUCH_TRESHOLD);
  setBehavior(0); // default behavior
  setName("Floower");
  setRemoteOnStartup(false);

  colorScheme[0] = colorWhite;
  colorScheme[1] = colorYellow;
  colorScheme[2] = colorOrange;
  colorScheme[3] = colorRed;
  colorScheme[4] = colorPink;
  colorScheme[5] = colorPurple;
  colorScheme[6] = colorBlue;
  colorScheme[7] = colorGreen;
  colorSchemeSize = 8;
  writeColorScheme();
}

void Config::setTouchTreshold(uint8_t touchTreshold) {
  this->touchTreshold = touchTreshold;
  EEPROM.write(EEPROM_ADDRESS_TOUCH_TRESHOLD, touchTreshold);
}

void Config::setBehavior(uint8_t behavior) {
  this->behavior = behavior;
  EEPROM.write(EEPROM_ADDRESS_BEHAVIOR, behavior);
}

void Config::setColorScheme(RgbColor* colors, uint8_t size) {
  this->colorSchemeSize = size;
  for (uint8_t i = 0; i < size; i++) {
    this->colorScheme[i] = colors[i];
  }
  writeColorScheme();
}

void Config::writeColorScheme() {
  int8_t address;
  for (uint8_t i = 0; i < colorSchemeSize && i < COLOR_SCHEME_MAX_LENGTH; i++) {
    address = EEPROM_ADDRESS_COLOR_SCHEME + i * 3;
    EEPROM.write(address + 0, colorScheme[i].R);
    EEPROM.write(address + 1, colorScheme[i].G);
    EEPROM.write(address + 2, colorScheme[i].B);
  }
  EEPROM.write(EEPROM_ADDRESS_COLOR_SCHEME_LENGTH, colorSchemeSize);
}

void Config::readColorScheme() {
  int8_t address;
  colorSchemeSize = min(EEPROM.read(EEPROM_ADDRESS_COLOR_SCHEME_LENGTH), (uint8_t) COLOR_SCHEME_MAX_LENGTH);
  for(uint8_t i = 0; i < colorSchemeSize; i++) {
    address = EEPROM_ADDRESS_COLOR_SCHEME + i * 3;
    colorScheme[i] = RgbColor(EEPROM.read(address + 0), EEPROM.read(address + 1), EEPROM.read(address + 2));
  }
}

void Config::setName(String name) {
  this->name = name;
  uint8_t length = min((uint8_t) name.length(), (uint8_t) NAME_MAX_LENGTH);
  for (uint8_t i = 0; i < length; i++) {
    EEPROM.write(EEPROM_ADDRESS_NAME + i, name[i]);
  }
  EEPROM.write(EEPROM_ADDRESS_NAME_LENGTH, length);
}

void Config::readName() {
  uint8_t length = min(EEPROM.read(EEPROM_ADDRESS_NAME_LENGTH), (uint8_t) NAME_MAX_LENGTH);
  char data[length + 1];

  for(uint8_t i = 0; i < length; i++) {
    data[i] = EEPROM.read(EEPROM_ADDRESS_NAME + i);
  }
  data[length] = '\0';
  name = String(data);
}

void Config::setRemoteOnStartup(boolean initRemoteOnStartup) {
  this->initRemoteOnStartup = initRemoteOnStartup;
  EEPROM.write(EEPROM_ADDRESS_INIT_REMOTE_ON_STARTUP, initRemoteOnStartup ? 1 : 0);
}

void Config::writeInt(unsigned int address, unsigned int value) {
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);
  
  EEPROM.write(address, two);
  EEPROM.write(address + 1, one);
}

unsigned int Config::readInt(unsigned int address) {
  byte two = EEPROM.read(address);
  unsigned int one = EEPROM.read(address + 1);
 
  return (two & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

void Config::commit() {
  EEPROM.commit();
}
