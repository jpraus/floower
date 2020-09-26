#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include <EEPROM.h>
#include <NeoPixelBus.h>

#define COLOR_SCHEME_MAX_LENGTH 10
#define NAME_MAX_LENGTH 25 // BLE name limit

// default values
#define DEFAULT_TOUCH_TRESHOLD 45 // lower means lower sensitivity (45 is normal)

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
    void setTouchTreshold(uint8_t touchTreshold);
    void setBehavior(uint8_t behavior);
    void setColorScheme(RgbColor* colors, uint8_t size);
    void setName(String name);
    void setRemoteOnStartup(boolean initRemoteOnStartup);
    void commit();

    unsigned int servoClosed = 1000; // default safe values
    unsigned int servoOpen = 1000;
    uint8_t hardwareRevision = 0;
    uint8_t firmwareVersion = 0;
    unsigned int serialNumber = 0;
    
    uint8_t touchTreshold = DEFAULT_TOUCH_TRESHOLD;
    uint8_t behavior = 0;
    uint8_t colorSchemeSize = 0;
    RgbColor colorScheme[10]; // max 10 colors
    String name;
    boolean initRemoteOnStartup = false;

  private:
    void writeColorScheme();
    void readColorScheme();
    void readName();
    void writeInt(unsigned int address, unsigned int value);
    unsigned int readInt(unsigned int address);

};

#endif
