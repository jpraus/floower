#include "Config.h"
#include "math.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Config";
#endif

// max 512B of EEPROM

#define EEPROM_SIZE 256
#define CONFIG_VERSION 5

#define FLAG_BIT_CALIBRATED 0
#define FLAG_BIT_BLUETOOTH_ALWAYS_ON 1
#define FLAG_BIT_TOUCH_CALIBRATED 2

// DO NOT CHANGE MEMORY ADDRESSES!
// Hardware constants (reserved 0-19)
#define EEPROM_ADDRESS_CONFIG_VERSION 0 // byte - version of configuration
#define EEPROM_ADDRESS_LEDS_MODEL 1 // 0 - WS2812b, 1 - SK6812 (1 byte) NOT USED ANYMORE (since version 1)
#define EEPROM_ADDRESS_SERVO_CLOSED 2 // integer (2 bytes) calibrated position of servo blossom closed (since version 1)
#define EEPROM_ADDRESS_SERVO_OPEN 4 // integer (2 bytes) calibrated position of servo blossom open (since version 1)
#define EEPROM_ADDRESS_REVISION 6 // byte - revision number of the logic board to enable features (since version 2)
#define EEPROM_ADDRESS_SERIALNUMBER 7 // integer (2 bytes) (since version 2)
#define EEPROM_ADDRESS_FLAGS 9 // byte (8 bites) - config flags [calibrated,bluetoothAlwaysOn,touchCalibrated,,,,,] (since version 3)

// Customizable values (20+)
#define EEPROM_ADDRESS_TOUCH_THRESHOLD 20 // byte (since version 2) - calibrated touch threshold value
#define EEPROM_ADDRESS_BEHAVIOR 21 // byte - enumeration of predefined behaviors (since version 2)
#define EEPROM_ADDRESS_COLOR_SCHEME_LENGTH 22 // 1 byte - number of colors set in EEPROM_ADDRESS_COLOR_SCHEME (since version 2)
#define EEPROM_ADDRESS_NAME_LENGTH 23 // byte - length of data stored in EEPROM_ADDRESS_NAME (since version 2)
#define EEPROM_ADDRESS_SPEED 24 // byte - speed of opening/closing in 0.1s (since version 4)
#define EEPROM_ADDRESS_MAX_OPEN_LEVEL 25 // byte - maximum open level in percents (0-100) (since version 4)
#define EEPROM_ADDRESS_COLOR_BRIGHTNESS 26 // byte - intensity of LEDs in percents (0-100), informative - should be already applied to color scheme (since version 4)
// TODO: touch threshold adjustment not used at the moment
//#define EEPROM_ADDRESS_TOUCH_THRESHOLD_ADJUSTMENT 27 // byte (since version 5) - adjusted touch threshold if needed by user
// 28 available
// 29 available
#define EEPROM_ADDRESS_COLOR_SCHEME 30 // (30-59) 30 bytes (15x HS set) - array of 2 bytes per stored HSB color, B is missing [(H/9 + S/7), (H/9 + S/7), ..] (since version 4)
#define EEPROM_ADDRESS_NAME 60 // (60-99) max 25 (40 reserved) chars (since version 2)

// wifi
#define EEPROM_ADDRESS_WIFI_SSID_LENGTH 100 // byte - length of data stored in EEPROM_ADDRESS_WIFI_SSID (since version 5)
#define EEPROM_ADDRESS_WIFI_SSID 101 // (101-132) max 32 characters (since version 5)
#define EEPROM_ADDRESS_WIFI_PWD_LENGTH 133 // byte - length of data stored in EEPROM_ADDRESS_WIFI_PWD (since version 5)
#define EEPROM_ADDRESS_WIFI_PWD 134 // (134-197) max 64 characters (since version 5)
#define EEPROM_ADDRESS_FLOUD_TOKEN_LENGTH 198 // byte - length of data stored in EEPROM_ADDRESS_FLOUD_TOKEN (since version 5)
#define EEPROM_ADDRESS_FLOUD_TOKEN 199 // (199-238) max 40 characters (since version 5)
// next available is 239

void Config::begin() {
    EEPROM.begin(EEPROM_SIZE);
}

void Config::load() {
    uint8_t configVersion = EEPROM.read(EEPROM_ADDRESS_CONFIG_VERSION);

    if (configVersion > 0 && configVersion < 255) {
        servoClosed = readInt(EEPROM_ADDRESS_SERVO_CLOSED);
        servoOpen = readInt(EEPROM_ADDRESS_SERVO_OPEN);

        // backward compatibility => reset to factory settings
        if (configVersion < 2) {
            hardwareCalibration(servoClosed, servoOpen, 0, 0);
            factorySettings();
        }

        // backward compatibility => set calibrated ON
        if (configVersion < 3) {
            setCalibrated();
        }

        // backward compatibility => settings values
        if (configVersion < 4) {
            resetColorScheme();
            hardwareRevision = EEPROM.read(EEPROM_ADDRESS_REVISION);
            EEPROM.write(EEPROM_ADDRESS_TOUCH_THRESHOLD, DEFAULT_TOUCH_THRESHOLD);
            EEPROM.write(EEPROM_ADDRESS_SPEED, DEFAULT_SPEED);
            EEPROM.write(EEPROM_ADDRESS_MAX_OPEN_LEVEL, DEFAULT_MAX_OPEN_LEVEL);
            EEPROM.write(EEPROM_ADDRESS_COLOR_BRIGHTNESS, DEFAULT_COLOR_BRIGHTNESS);
        }

        // backward compatibility => wifi settings
        if (configVersion < 5) {
            setWifi("", "");
            setFloud("");
        }

        if (configVersion < CONFIG_VERSION) {
            ESP_LOGW(LOG_TAG, "Config outdated %d -> %d", configVersion, CONFIG_VERSION);
            EEPROM.write(EEPROM_ADDRESS_CONFIG_VERSION, CONFIG_VERSION);
            commit(); // this will commit also changes above
        }

        hardwareRevision = EEPROM.read(EEPROM_ADDRESS_REVISION);
        serialNumber = readInt(EEPROM_ADDRESS_SERIALNUMBER);
        touchThreshold = EEPROM.read(EEPROM_ADDRESS_TOUCH_THRESHOLD);
        readFlags();
        readColorScheme();
        readName();
        readSpeed();
        readMaxOpenLevel();
        readColorBrightness();
        readWifiAndFloud();
      
        ESP_LOGI(LOG_TAG, "Config ready");
        ESP_LOGI(LOG_TAG, "HW: %d -> %d, R%d, SN%d, f%d, tt%d", servoClosed, servoOpen, hardwareRevision, serialNumber, flags, touchThreshold);
        ESP_LOGI(LOG_TAG, "Flags: bt%d, %s", bluetoothAlwaysOn, name.c_str());
        ESP_LOGI(LOG_TAG, "Sett: spd%d, mol%d, brg%d", speed, maxOpenLevel, colorBrightness);
        for (uint8_t i = 0; i < colorSchemeSize; i++) {
            ESP_LOGI(LOG_TAG, "Color %d: %.2f,%.2f", i, colorScheme[i].H, colorScheme[i].S);
        }
        ESP_LOGI(LOG_TAG, "WiFi: %s, p%d, t%d", wifiSsid.c_str(), !wifiPassword.isEmpty(), !floudToken.isEmpty());
    }
    else {
        ESP_LOGE(LOG_TAG, "Not configured");
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
    EEPROM.write(EEPROM_ADDRESS_FLAGS, 0);

    this->servoClosed = servoClosed;
    this->servoOpen = servoOpen;
    this->hardwareRevision = hardwareRevision;
    this->serialNumber = serialNumber;
}

void Config::factorySettings() {
    ESP_LOGI(LOG_TAG, "Factory reset");
    setName("Floower");
    setBluetoothAlwaysOn(false);
    setSpeed(DEFAULT_SPEED);
    setMaxOpenLevel(DEFAULT_MAX_OPEN_LEVEL);
    setColorBrightness(DEFAULT_COLOR_BRIGHTNESS);
    EEPROM.write(EEPROM_ADDRESS_TOUCH_THRESHOLD, DEFAULT_TOUCH_THRESHOLD); // not used, for forward compabitility only
    EEPROM.write(EEPROM_ADDRESS_BEHAVIOR, DEFAULT_BEHAVIOR); // not used, for forward compabitility only
    resetColorScheme();
}

void Config::resetColorScheme() {
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

void Config::readFlags() {
    flags = EEPROM.read(EEPROM_ADDRESS_FLAGS);
    calibrated = CHECK_BIT(flags, FLAG_BIT_CALIBRATED);
    bluetoothAlwaysOn = CHECK_BIT(flags, FLAG_BIT_BLUETOOTH_ALWAYS_ON);
    touchCalibrated = CHECK_BIT(flags, FLAG_BIT_TOUCH_CALIBRATED);
}

void Config::setCalibrated() {
    flags = SET_BIT(flags, FLAG_BIT_CALIBRATED);
    EEPROM.write(EEPROM_ADDRESS_FLAGS, flags);
    this->calibrated = true;
}

void Config::setBluetoothAlwaysOn(bool bluetoothAlwaysOn) {
    flags = bluetoothAlwaysOn ? SET_BIT(flags, FLAG_BIT_BLUETOOTH_ALWAYS_ON) : CLEAR_BIT(flags, FLAG_BIT_BLUETOOTH_ALWAYS_ON);
    EEPROM.write(EEPROM_ADDRESS_FLAGS, flags);
    this->bluetoothAlwaysOn = bluetoothAlwaysOn;
}

void Config::setTouchCalibrated(bool touchCalibrated) {
    flags = touchCalibrated ? SET_BIT(flags, FLAG_BIT_TOUCH_CALIBRATED) : CLEAR_BIT(flags, FLAG_BIT_TOUCH_CALIBRATED);
    EEPROM.write(EEPROM_ADDRESS_FLAGS, flags);
    this->touchCalibrated = touchCalibrated;
}

void Config::setColorScheme(HsbColor* colors, uint8_t size) {
    this->colorSchemeSize = size;
    for (uint8_t i = 0; i < size; i++) {
        this->colorScheme[i] = colors[i];
    }
    writeColorScheme();
}

void Config::setTouchThreshold(uint8_t touchThreshold) {
    EEPROM.write(EEPROM_ADDRESS_TOUCH_THRESHOLD, touchThreshold);
    this->touchThreshold = touchThreshold;
}

void Config::setSpeed(uint8_t speed) {
    this->speed = speed;
    this->speedMillis = speed * 100;
    EEPROM.write(EEPROM_ADDRESS_SPEED, speed);
}

void Config::setMaxOpenLevel(uint8_t maxOpenLevel) {
    this->maxOpenLevel = maxOpenLevel;
    EEPROM.write(EEPROM_ADDRESS_MAX_OPEN_LEVEL, maxOpenLevel);
}

void Config::setColorBrightness(uint8_t colorBrightness) {
    this->colorBrightness = colorBrightness;
    this->colorBrightnessDecimal = (double) colorBrightness / 100.0;
    EEPROM.write(EEPROM_ADDRESS_COLOR_BRIGHTNESS, colorBrightness);
}

void Config::readSpeed() {
    speed = EEPROM.read(EEPROM_ADDRESS_SPEED);
    if (speed < 5) {
        speed = 5;
    }
    speedMillis = speed * 100;
}

void Config::readMaxOpenLevel() {
    maxOpenLevel = EEPROM.read(EEPROM_ADDRESS_MAX_OPEN_LEVEL);
    if (maxOpenLevel > 100) {
        maxOpenLevel = 100;
    }
}

void Config::readColorBrightness() {
    colorBrightness = EEPROM.read(EEPROM_ADDRESS_COLOR_BRIGHTNESS);
    if (colorBrightness > 100) {
        colorBrightness = 100;
    }
    colorBrightnessDecimal = (double) colorBrightness / 100.0;
}

void Config::writeColorScheme() {
    for (uint8_t i = 0; i < colorSchemeSize && i < COLOR_SCHEME_MAX_LENGTH; i++) {
        writeInt(EEPROM_ADDRESS_COLOR_SCHEME + i * 2, encodeHSColor(colorScheme[i].H, colorScheme[i].S));
    }
    EEPROM.write(EEPROM_ADDRESS_COLOR_SCHEME_LENGTH, colorSchemeSize);
}

void Config::readColorScheme() {
    colorSchemeSize = min(EEPROM.read(EEPROM_ADDRESS_COLOR_SCHEME_LENGTH), (uint8_t) COLOR_SCHEME_MAX_LENGTH);
    for(uint8_t i = 0; i < colorSchemeSize; i++) {
        colorScheme[i] = decodeHSColor(readInt(EEPROM_ADDRESS_COLOR_SCHEME + i * 2));
    }
}

uint16_t Config::encodeHSColor(double hue, double saturation) {
    uint16_t valueH = hue * 360;
    uint8_t valueS = saturation * 100;
    return (valueH << 7) | (valueS & 0x7F);
}

HsbColor Config::decodeHSColor(uint16_t valueHS) {
    double saturation = valueHS & 0x7F;
    saturation = saturation / 100.0;
    double hue = valueHS >> 7;
    hue = hue / 360.0;
    return HsbColor(hue, saturation, 1.0);
}

void Config::setName(String name) {
    this->name = name;
    writeString(EEPROM_ADDRESS_NAME, name, EEPROM_ADDRESS_NAME_LENGTH, NAME_MAX_LENGTH);
}

void Config::readName() {
    name = readString(EEPROM_ADDRESS_NAME, EEPROM_ADDRESS_NAME_LENGTH, NAME_MAX_LENGTH);
}

void Config::setWifi(String ssid, String password) {
    this->wifiSsid = ssid;
    this->wifiPassword = password;
    writeString(EEPROM_ADDRESS_WIFI_SSID, ssid, EEPROM_ADDRESS_WIFI_SSID_LENGTH, WIFI_SSID_MAX_LENGTH);
    writeString(EEPROM_ADDRESS_WIFI_PWD, password, EEPROM_ADDRESS_WIFI_PWD_LENGTH, WIFI_PWD_MAX_LENGTH);
    wifiChanged = true;
}

void Config::setFloud(String token) {
    this->floudToken = token;
    writeString(EEPROM_ADDRESS_FLOUD_TOKEN, token, EEPROM_ADDRESS_FLOUD_TOKEN_LENGTH, FLOUD_TOKEN_MAX_LENGTH);
    wifiChanged = true;
}

void Config::readWifiAndFloud() {
    wifiSsid = readString(EEPROM_ADDRESS_WIFI_SSID, EEPROM_ADDRESS_WIFI_SSID_LENGTH, WIFI_SSID_MAX_LENGTH);
    wifiPassword = readString(EEPROM_ADDRESS_WIFI_PWD, EEPROM_ADDRESS_WIFI_PWD_LENGTH, WIFI_PWD_MAX_LENGTH);
    floudToken = readString(EEPROM_ADDRESS_FLOUD_TOKEN, EEPROM_ADDRESS_FLOUD_TOKEN_LENGTH, FLOUD_TOKEN_MAX_LENGTH);
}

void Config::writeInt(uint16_t address, uint16_t value) {
    uint8_t two = (value & 0xFF);
    uint8_t one = ((value >> 8) & 0xFF);
    
    EEPROM.write(address, two);
    EEPROM.write(address + 1, one);
}

uint16_t Config::readInt(uint16_t address) {
    uint8_t two = EEPROM.read(address);
    uint16_t one = EEPROM.read(address + 1);
  
    return (two & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

void Config::writeString(uint16_t address, String value, uint16_t sizeAddress, uint8_t maxLength) {
    uint8_t length = min((uint8_t) value.length(), (uint8_t) maxLength);
    for (uint8_t i = 0; i < length; i++) {
        EEPROM.write(address + i, value[i]);
    }
    EEPROM.write(sizeAddress, length);
}

String Config::readString(uint16_t address, uint16_t sizeAddress, uint8_t maxLength) {
    uint8_t length = min(EEPROM.read(sizeAddress), (uint8_t) maxLength);
    char data[length + 1];

    for(uint8_t i = 0; i < length; i++) {
        data[i] = EEPROM.read(address + i);
    }
    data[length] = '\0';
    return String(data);
}

void Config::commit() {
    EEPROM.commit();
    if (configChangedCallback != nullptr) {
        configChangedCallback(wifiChanged);
        wifiChanged = false;
    }
}

void Config::onConfigChanged(ConfigChangedCallback callback) {
    configChangedCallback = callback;
}