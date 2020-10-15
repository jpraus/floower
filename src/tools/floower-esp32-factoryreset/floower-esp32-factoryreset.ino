#include "Arduino.h"
#include <EEPROM.h>

#define EEPROM_SIZE 128

void setup() {
  Serial.begin(115200);
  Serial.print("Floower Factory Reset ... ");
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 128; i ++) {
    EEPROM.write(i, 255);
  }
  EEPROM.commit();
  Serial.println("done");
}

void loop() {
  // nop
}
