#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include <EEPROM.h>
#include <NeoPixelBus.h>

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
#define EEPROM_ADDRESS_COLOR_SCHEME 30 // (30-59) 30 bytes (10x RGB set) - list of up to 10 colors (since version 2)
#define EEPROM_ADDRESS_NAME 60 // (60-99) max 25 (40 reserved) chars (since version 2)

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

  private:
    void writeColorScheme();
    void readColorScheme();
    void readName();
    void writeInt(unsigned int address, unsigned int value);
    unsigned int readInt(unsigned int address);

};

#endif
