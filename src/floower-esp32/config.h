#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include <EEPROM.h>
#include <NeoPixelBus.h>

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))
#define SET_BIT(var, pos) ((var) | (1<<(pos)))
#define CLEAR_BIT(var, pos) ((var) & ~(1<<(pos)))

#define COLOR_SCHEME_MAX_LENGTH 10
#define NAME_MAX_LENGTH 25 // BLE name limit

// default values
#define DEFAULT_TOUCH_THRESHOLD 45 // lower means lower sensitivity (45 is normal)

// default intensity is 70% (178)
const RgbColor colorRed(156, 0, 0);
const RgbColor colorGreen(40, 178, 0);
const RgbColor colorBlue(0, 65, 178);
const RgbColor colorYellow(178, 170, 0);
const RgbColor colorOrange(178, 64, 0);
const RgbColor colorWhite(178);
const RgbColor colorPurple(148, 0, 178);
const RgbColor colorPink(178, 0, 73);
const RgbColor colorBlack(0);

class Config {
  public:
    Config(uint8_t firmwareVersion) : firmwareVersion(firmwareVersion) {}
    void begin();
    void load();
    void hardwareCalibration(unsigned int servoClosed, unsigned int servoOpen, uint8_t hardwareRevision, unsigned int serialNumber);
    void factorySettings();
    void setTouchThreshold(uint8_t touchThreshold);
    void setBehavior(uint8_t behavior);
    void setColorScheme(RgbColor* colors, uint8_t size);
    void setName(String name);
    void setRemoteOnStartup(bool initRemoteOnStartup);
    void setCalibrated();
    void commit();

    unsigned int servoClosed = 1000; // default safe values
    unsigned int servoOpen = 1000;
    uint8_t hardwareRevision = 0;
    uint8_t firmwareVersion = 0;
    unsigned int serialNumber = 0;

    // extracted flags
    bool initRemoteOnStartup = false;
    bool calibrated = false;

    uint8_t touchThreshold = DEFAULT_TOUCH_THRESHOLD;
    uint8_t behavior = 0;
    uint8_t colorSchemeSize = 0;
    RgbColor colorScheme[10]; // max 10 colors
    String name;

  private:
    void readFlags();
    void writeColorScheme();
    void readColorScheme();
    void readName();
    void writeInt(unsigned int address, unsigned int value);
    unsigned int readInt(unsigned int address);

    uint8_t flags = 0;

};

#endif
