#pragma once

#include "Arduino.h"
#include <EEPROM.h>
#include "NeoPixelBus.h"

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))
#define SET_BIT(var, pos) ((var) | (1<<(pos)))
#define CLEAR_BIT(var, pos) ((var) & ~(1<<(pos)))

#define COLOR_SCHEME_MAX_LENGTH 10
#define NAME_MAX_LENGTH 25 // BLE name limit

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

typedef struct Personification {
  uint8_t touchThreshold; // read-only, TODO: not used, kept for backward compatibility
  uint8_t behavior; // read-write
  uint8_t speed; // in 0.1s, read-write
  uint8_t maxOpenLevel; // 0-100, read-write
  uint8_t colorBrightness; // 0-100, read-write
} Personification;

class Config {
  public:
    Config(uint8_t firmwareVersion) : firmwareVersion(firmwareVersion) {}
    void begin();
    void load();
    void hardwareCalibration(uint8_t hardwareRevision, unsigned int serialNumber);
    void factorySettings();
    void resetColorScheme();
    void setColorScheme(HsbColor* colors, uint8_t size);
    void setTouchThreshold(uint8_t touchThreshold);
    void setName(String name);
    void setRemoteOnStartup(bool initRemoteOnStartup);
    void setCalibrated();
    void setPersonification(Personification personification);
    void commit();

    static uint16_t encodeHSColor(double hue, double saturation);
    static HsbColor decodeHSColor(uint16_t valueHS);

    // calibration
    unsigned int servoClosed = 1000; // default safe values
    unsigned int servoOpen = 1000;
    uint8_t hardwareRevision = 0;
    uint8_t firmwareVersion = 0;
    unsigned int serialNumber = 0;

    // extracted flags
    bool initRemoteOnStartup = false;
    bool calibrated = false;

    // feature flags
    bool bluetoothEnabled = false;
    bool rainbowEnabled = false;
    bool touchEnabled = false;
    // configration
    uint8_t colorSchemeSize = 0;
    HsbColor colorScheme[10]; // max 10 colors
    uint8_t touchThreshold; // read-only
    String name;
    Personification personification;
    unsigned int speedMillis; // read-only, precalculated speed in ms
    double colorBrightness; // read-only, precalcuated color brightness (0.0-1.0)

  private:
    void readFlags();
    void writeColorScheme();
    void readColorScheme();
    void readName();
    void readPersonification();

    void writeInt(uint16_t address, uint16_t value);
    uint16_t readInt(uint16_t address);

    uint8_t flags = 0;

};