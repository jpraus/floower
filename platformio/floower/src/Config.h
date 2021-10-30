#pragma once

#include "Arduino.h"
#include <EEPROM.h>
#include "NeoPixelBus.h"

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))
#define SET_BIT(var, pos) ((var) | (1<<(pos)))
#define CLEAR_BIT(var, pos) ((var) & ~(1<<(pos)))

#define COLOR_SCHEME_MAX_LENGTH 10
#define NAME_MAX_LENGTH 25 // BLE name limit
#define WIFI_SSID_MAX_LENGTH 32
#define WIFI_PWD_MAX_LENGTH 64
#define FLOUD_TOKEN_MAX_LENGTH 40
#define FLOUD_DEVICE_ID_MAX_LENGTH 40

// default values
#define DEFAULT_TOUCH_THRESHOLD 45 // lower means lower sensitivity (45 is normal)
#define DEFAULT_BEHAVIOR 0
#define DEFAULT_SPEED 50 // x0.1s = 5 seconds to open/close
#define DEFAULT_MAX_OPEN_LEVEL 100 // default open level is 100%
#define DEFAULT_COLOR_BRIGHTNESS 70 // default intensity is 70%

const HsbColor colorRed(0.0, 1.0, 1.0);
const HsbColor colorGreen(0.3, 1.0, 1.0);
const HsbColor colorBlue(0.61, 1.0, 1.0);
const HsbColor colorYellow(0.16, 1.0, 1.0);
const HsbColor colorOrange(0.06, 1.0, 1.0);
const HsbColor colorWhite(0.0, 0.0, 1.0);
const HsbColor colorPurple(0.81, 1.0, 1.0);
const HsbColor colorPink(0.93, 1.0, 1.0);
const HsbColor colorBlack(0.0, 1.0, 0.0);

typedef std::function<void(bool wifiChanged)> ConfigChangedCallback;

class Config {
    public:
        Config(uint8_t firmwareVersion) : firmwareVersion(firmwareVersion) {}
        void begin();
        void load();
        void hardwareCalibration(unsigned int servoClosed, unsigned int servoOpen, uint8_t hardwareRevision, unsigned int serialNumber);
        void factorySettings();
        void resetColorScheme();
        void setColorScheme(HsbColor* colors, uint8_t size);
        void setTouchThreshold(uint8_t touchThreshold);
        void setName(String name);
        void setBluetoothAlwaysOn(bool bluetoothAlwaysOn);
        void setCalibrated();
        void setTouchCalibrated(bool touchCalibrated);
        void setSpeed(uint8_t speed);
        void setMaxOpenLevel(uint8_t maxOpenLevel);
        void setColorBrightness(uint8_t colorBrightness);
        void setWifi(String ssid, String password);
        void setFloud(String deviceId, String token);
        void commit();
        void onConfigChanged(ConfigChangedCallback callback);

        static uint16_t encodeHSColor(double hue, double saturation);
        static HsbColor decodeHSColor(uint16_t valueHS);

        // calibration
        unsigned int servoClosed = 1000; // default safe values
        unsigned int servoOpen = 1000;
        uint8_t hardwareRevision = 0;
        uint8_t firmwareVersion = 0;
        unsigned int serialNumber = 0;

        // extracted flags
        bool bluetoothAlwaysOn = false;
        bool calibrated = false;
        bool touchCalibrated = false;

        // feature flags
        bool deepSleepEnabled = false;
        bool bluetoothEnabled = false;
        bool wifiEnabled = false;
        bool colorPickerEnabled = false;

        // configration
        uint8_t colorSchemeSize = 0;
        HsbColor colorScheme[10]; // max 10 colors
        uint8_t touchThreshold; // read-only, calibrated at the beginning
        String name;
        uint8_t speed;
        uint16_t speedMillis; // read-only, precalculated speed in ms
        uint8_t colorBrightness;
        double colorBrightnessDecimal; // read-only, precalcuated color brightness (0.0-1.0)
        uint8_t maxOpenLevel;
        String wifiSsid;
        String wifiPassword;
        String floudDeviceId;
        String floudToken;

    private:
        void readFlags();
        void writeColorScheme();
        void readColorScheme();
        void readName();
        void readWifiAndFloud();
        void readSpeed();
        void readMaxOpenLevel();
        void readColorBrightness();

        void writeInt(uint16_t address, uint16_t value);
        uint16_t readInt(uint16_t address);

        void writeString(uint16_t address, String value, uint16_t sizeAddress, uint8_t maxLength);
        String readString(uint16_t address, uint16_t sizeAddress, uint8_t maxLength);

        uint8_t flags = 0;
        ConfigChangedCallback configChangedCallback;
        bool wifiChanged = false;

};