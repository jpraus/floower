/**
 * TODO:
 * - refactor WiFi connectivity using events API
 * - move remote control and UDP commons to separate class
 */

#include <EEPROM.h>
//#include <esp_wifi.h>
//#include <WiFi.h>
#include <esp_task_wdt.h>
#include "floower.h"
#include "remote.h"

bool remoteEnabled = true;
bool deepSleepEnabled = true;

///////////// PERSISTENT CONFIGURATION

// never ever turn the write configuration flag you will overwrite the hardware settings and probably break the flower
boolean writeConfiguration = false;

const byte CONFIG_VERSION = 1;
unsigned int configServoClosed = 800; // 650
unsigned int configServoOpen = configServoClosed + 500; // 700
byte configLedsModel = LEDS_MODEL_WS2812B;
//byte configLedsModel = LEDS_MODEL_SK6812;

#define EEPROM_SIZE 10 // 10 bytes

// do not change!
#define MEMORY_VERSION 0
#define MEMORY_LEDS_MODEL 1 // 0 - WS2812b, 1 - SK6812 (1 byte)
#define MEMORY_SERVO_CLOSED 2 // integer (2 bytes)
#define MEMORY_SERVO_OPEN 4 // integer (2 bytes)
#define MEMORY_LEAF_SENSITIVTY 6 // 0-255 (1 byte)

///////////// REMOTE CONNECTION



///////////// POWER MODE

// tuned for 1600mAh LIPO battery
#define POWER_LOW_ENTER_THRESHOLD 0 // enter state when voltage drop below this threshold (3.55)
#define POWER_LOW_LEAVE_THRESHOLD 0 // leave state when voltage rise above this threshold (3.6)
#define POWER_DEAD_THRESHOLD 3.4

///////////// STATE OF FLOWER

#define STATE_INIT 0
#define STATE_WAKEUP 1
#define STATE_STANDBY 2
#define STATE_LIT 3
#define STATE_BLOOMED 4
#define STATE_CLOSED 5

#define STATE_BATTERYDEAD 10
#define STATE_SHUTDOWN 11

byte state = STATE_INIT;
bool colorPickerOn = false;

long changeStateTime = 0;
byte changeStateTo;

RgbColor colors[] = {
  colorWhite,
  colorYellow,
  colorOrange,
  colorRed,
  colorPink,
  colorPurple,
  colorBlue
};
const byte colorsCount = 7; // must match the size of colors array!
long colorsUsed = 0;

///////////// CODE

#define DEEP_SLEEP_INACTIVITY_TIMEOUT 20000 // fall in deep sleep after timeout
#define BATTERY_DEAD_WARNING_DURATION 5000 // how long to show battery dead status
#define PERIODIC_OPERATIONS_INTERVAL 5000

long periodicOperationsTime = 0;
long initRemoteTime = 0;

Floower floower;
Remote remote(&floower);

void setup() {
  Serial.begin(115200);
  Serial.println("Tulip INITIALIZING");
  configure();
  changeState(STATE_STANDBY);

  // after wake up setup
  bool wasSleeping = false;
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeup_reason) {
    Serial.println("Waking up after Deep Sleep");
    floower.touchISR();
    wasSleeping = true;
  }

  // init hardware
  //esp_wifi_stop();
  //btStop();
  floower.init(configLedsModel);
  floower.readBatteryVoltage(); // calibrate the ADC
  floower.onLeafTouch(onLeafTouch);
  Floower::touchAttachInterruptProxy([](){ floower.touchISR(); });
  delay(50); // wait for init

  // check if there is enough power to run
  bool isBatteryDead = false;
  if (!floower.isUSBPowered()) {
    float voltage = floower.readBatteryVoltage();
    if (voltage < POWER_DEAD_THRESHOLD) {
      delay(500);
      voltage = floower.readBatteryVoltage(); // re-verify the voltage after .5s
      isBatteryDead = voltage < POWER_DEAD_THRESHOLD;
    }
  }

  if (isBatteryDead) {
    // battery is dead, do not wake up, shutdown after a status color
    Serial.println("Battery is dead, shutting down");
    changeState(STATE_BATTERYDEAD);
    planChangeState(STATE_SHUTDOWN, BATTERY_DEAD_WARNING_DURATION);
    floower.setLowPowerMode(true);
    floower.setColor(colorRed, FloowerColorMode::PULSE, 1000);
  }
  else {
    // normal operation
    floower.initServo(configServoClosed, configServoOpen);
    if (!wasSleeping) {
      floower.setPetalsOpenLevel(0, 100);
    }
  }

  if (remoteEnabled) {
    initRemoteTime = millis() + 5000; // defer init of BLE by 5 seconds
  }

  periodicOperationsTime = millis() + PERIODIC_OPERATIONS_INTERVAL; // TODO millis overflow
  Serial.println("Tulip READY");
}

void loop() {
  floower.update();
  remote.update();

  // timers
  long now = millis();
  if (periodicOperationsTime > 0 && periodicOperationsTime < now) {
    periodicOperationsTime = now + PERIODIC_OPERATIONS_INTERVAL;
    periodicOperation();
  }
  if (changeStateTime > 0 && changeStateTime < now) {
    changeStateTime = 0;
    changeState(changeStateTo);
  }
  if (initRemoteTime > 0 && initRemoteTime < now) {
    initRemoteTime = 0;
    remote.init();
  }

  // update state machine
  if (state == STATE_WAKEUP) {
    floower.setColor(nextRandomColor(), FloowerColorMode::TRANSITION, 5000);
    changeState(STATE_LIT);
  }
  else if (state == STATE_SHUTDOWN && !floower.arePetalsMoving()) {
    enterDeepSleep();
  }

  // save some power when flower is idle
  if (floower.isIdle()) {
    delay(10);
  }
}

void periodicOperation() {
  floower.acty();
  powerWatchDog();
}

void planChangeState(byte newState, long timeout) {
  changeStateTo = newState;
  changeStateTime = millis() + timeout;
  Serial.print("Planned change state: ");
  Serial.print(newState);
  Serial.print(" in ");
  Serial.println(timeout);
}

void changeState(byte newState) {
  changeStateTime = 0;
  if (state != newState) {
    state = newState;
    Serial.print("Change state: ");
    Serial.println(newState);

    if (newState == STATE_STANDBY && deepSleepEnabled && !remoteEnabled) {
      planChangeState(STATE_SHUTDOWN, DEEP_SLEEP_INACTIVITY_TIMEOUT);
    }
  }
}

void onLeafTouch(FloowerTouchType touchType) {
  if (state == STATE_BATTERYDEAD || state == STATE_SHUTDOWN) {
    return;
  }

  switch (touchType) {
    case RELEASE:
      if (colorPickerOn) {
        floower.stopColorPicker();
        colorPickerOn = false;
      }
      else if (floower.isIdle()) {
        if (state == STATE_STANDBY) {
          // open + set color
          floower.setColor(nextRandomColor(), FloowerColorMode::TRANSITION, 5000);
          floower.setPetalsOpenLevel(100, 5000);
          changeState(STATE_BLOOMED);
        }
        else if (state == STATE_LIT) {
          // open
          floower.setPetalsOpenLevel(100, 5000);
          changeState(STATE_BLOOMED);
        }
        else if (state == STATE_BLOOMED) {
          // close
          floower.setPetalsOpenLevel(0, 5000);
          changeState(STATE_CLOSED);
        }
        else if (state == STATE_CLOSED) {
          // shutdown
          floower.setColor(colorBlack, FloowerColorMode::TRANSITION, 2000);
          changeState(STATE_STANDBY);
        }
      }
      break;

    case HOLD:
      floower.startColorPicker();
      colorPickerOn = true;
      if (state == STATE_STANDBY) {
        changeState(STATE_LIT);
      }
      break;
  }
}

void powerWatchDog() {
  if (state == STATE_BATTERYDEAD || state == STATE_SHUTDOWN) {
    return;
  }

  float voltage = floower.readBatteryVoltage();
  if (floower.isUSBPowered()) {
    floower.setLowPowerMode(false);
  }
  else if (voltage < POWER_DEAD_THRESHOLD) {
    Serial.print("Shutting down, battery is dead (");
    Serial.print(voltage);
    Serial.println("V)");
    floower.setColor(colorBlack, FloowerColorMode::TRANSITION, 2500);
    floower.setPetalsOpenLevel(0, 2500);
    changeState(STATE_SHUTDOWN);
  }
  else if (!floower.isLowPowerMode() && voltage < POWER_LOW_ENTER_THRESHOLD) {
    Serial.print("Entering low power mode (");
    Serial.print(voltage);
    Serial.println("V)");
    floower.setLowPowerMode(true);
  }
  else if (floower.isLowPowerMode() && voltage >= POWER_LOW_LEAVE_THRESHOLD) {
    Serial.print("Leaving low power mode (");
    Serial.print(voltage);
    Serial.println("V)");
    floower.setLowPowerMode(false);
  }
}

void enterDeepSleep() {
  // TODO: move to floower class
  touchAttachInterrupt(4, [](){}, 50); // register interrupt to enable wakeup
  esp_sleep_enable_touchpad_wakeup();
  Serial.println("Going to sleep now");
  //esp_wifi_stop();
  btStop();
  esp_deep_sleep_start();
}

RgbColor nextRandomColor() {
  if (colorsUsed > 0) {
    long maxColors = pow(2, colorsCount) - 1;
    if (maxColors == colorsUsed) {
      colorsUsed = 0; // all colors used, reset
    }
  }

  byte colorIndex;
  long colorCode;
  int maxIterations = colorsCount * 3;

  do {
    colorIndex = random(0, colorsCount);
    colorCode = 1 << colorIndex;
    maxIterations--;
  } while ((colorsUsed & colorCode) > 0 && maxIterations > 0); // already used before all the rest colors

  colorsUsed += colorCode;
  return colors[colorIndex];
}

// remote control (not used)
/*
void remoteControl() {
  // TODO rewrite to async
  switch (remoteState) {
    case REMOTE_STATE_INIT:
      if (remoteMaster) {
        // server
        Serial.print("Configuring access point ... ");
        WiFi.softAP(ssid, password, 1, 0, 8);
        Serial.println("OK");

        udp.connect(WiFi.softAPIP(), UDP_PORT);
        Serial.print("UDP Server started on IP: ");
        Serial.println(WiFi.softAPIP());

        remoteState = REMOTE_STATE_AP;
        remoteActive = true;
      }
      else {
        // client
        Serial.println("Starting WiFi");
        remoteState = REMOTE_STATE_DISCONNECTED;
      }
      break;

    case REMOTE_STATE_AP:
      break; // nothing

    case REMOTE_STATE_DISCONNECTED:
      WiFi.mode(WIFI_STA);
      //wifi_station_connect();
      WiFi.begin(ssid, password);
      Serial.println("Connecting to WiFi ...");

      remoteState = REMOTE_STATE_CONNECTING;
      break;

    case REMOTE_STATE_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");

        if (udp.listen(UDP_PORT)) {
          Serial.print("UDP Listening on IP: ");
          Serial.println(WiFi.localIP());
          udp.onPacket(udpPacketReceived);
        }

        remoteActive = true;
        remoteState = REMOTE_STATE_CONNECTED;
      }
      break;

    case REMOTE_STATE_CONNECTED:
      if (reconnectTimer <= 0) {
        reconnectTimer = CHECK_CONNECTION_SECONDS;
        if (WiFi.status() != WL_CONNECTED) {
          remoteActive = false;
          remoteState = REMOTE_STATE_DISCONNECTED;
          Serial.println("Disconnected from WiFi");
        }
      }
      break;
  }
}

void broadcastMasterState() {
  if (remoteActive && remoteMaster) {
    CommandData command;
    // TODO
    //command.blossomOpenness = petalsOpenness;
    //command.red = newRGB[RED];
    //command.green = newRGB[GREEN];
    //command.blue = newRGB[BLUE];
    broadcastRemoteCommand(command);
  }
}

void broadcastRemoteCommand(CommandData command) {
  CommandPacket packet;

  Serial.print("Broadcasting command. Blossom: ");
  Serial.print(command.blossomOpenness);
  Serial.print(", Red: ");
  Serial.print(command.red);
  Serial.print(", Green: ");
  Serial.print(command.green);
  Serial.print(", Blue: ");
  Serial.println(command.blue);

  packet.data = command;
  udp.broadcastTo(packet.packet, PACKET_DATA_SIZE, UDP_PORT);
}

void udpPacketReceived(AsyncUDPPacket packet) {
  CommandPacket command;

  Serial.print("UDP Packet Type: ");
  Serial.print(packet.isBroadcast() ? "Broadcast" : (packet.isMulticast() ? "Multicast" : "Unicast"));
  Serial.print(", From: ");
  Serial.print(packet.remoteIP());
  Serial.print(":");
  Serial.print(packet.remotePort());
  Serial.print(", To: ");
  Serial.print(packet.localIP());
  Serial.print(":");
  Serial.print(packet.localPort());
  Serial.print(", Length: ");
  Serial.println(packet.length());

  //command.packet = packet.data();
  memcpy(command.packet, packet.data(), packet.length());
  commmandReceived(command.data);
}

void commmandReceived(CommandData command) {
  if (remoteMode) {
    Serial.print("Blossom: ");
    Serial.print(command.blossomOpenness);
    Serial.print(", Red: ");
    Serial.print(command.red);
    Serial.print(", Green: ");
    Serial.print(command.green);
    Serial.print(", Blue: ");
    Serial.println(command.blue);

    floower.setPetalsOpenLevel(command.blossomOpenness, 500); // TODO some default speed
    floower.setColor(RgbColor(command.red, command.green, command.blue), FloowerColorMode::TRANSITION, 500); // TODO some default speed
  }
}
*/
// configuration

void configure() {
  EEPROM.begin(EEPROM_SIZE);

  if (writeConfiguration) {
    Serial.println("New configuration to store to flash memory");

    EEPROM.write(MEMORY_VERSION, CONFIG_VERSION);
    EEPROM.write(MEMORY_LEDS_MODEL, configLedsModel);
    EEPROMWriteInt(MEMORY_SERVO_CLOSED, configServoClosed);
    EEPROMWriteInt(MEMORY_SERVO_OPEN, configServoOpen);
    EEPROM.commit();

    writeConfiguration = false;
  }
  else {
    byte version = EEPROM.read(MEMORY_VERSION);
    Serial.println(version);
    if (version == 0) {
      Serial.println("No configuration, using fallbacks");

      // some safe defaults to prevent damage to flower
      configLedsModel = 0;
      configServoClosed = 1000;
      configServoOpen = 1000;
    }
    else {
      configLedsModel = EEPROM.read(MEMORY_LEDS_MODEL);
      configServoClosed = EEPROMReadInt(MEMORY_SERVO_CLOSED);
      configServoOpen = EEPROMReadInt(MEMORY_SERVO_OPEN);
    }
  }

  Serial.println("Configuration ready:");
  Serial.print("LEDs: ");
  Serial.println(configLedsModel == 0 ? "WS2812b" : "SK6812");
  Serial.print("Servo closed: ");
  Serial.println(configServoClosed);
  Serial.print("Servo open: ");
  Serial.println(configServoOpen);
}

void EEPROMWriteInt(int address, int value) {
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);
  
  EEPROM.write(address, two);
  EEPROM.write(address + 1, one);
}

int EEPROMReadInt(int address) {
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);
 
  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}
