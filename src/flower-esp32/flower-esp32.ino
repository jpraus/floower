/**
 * TODO:
 * - refactor WiFi connectivity using events API
 * - move remote controle and UDP comm to separate class
 */

#include <EEPROM.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include "flower.h"

bool remoteMode = false; // not used
bool remoteMaster = false; // not used
bool deepSleepEnabled = true;

///////////// PERSISTENT CONFIGURATION

boolean writeConfiguration = false;

const byte CONFIG_VERSION = 1;
unsigned int configServoClosed = 650;
unsigned int configServoOpen = configServoClosed + 700;
byte configLedsModel = LEDS_MODEL_WS2812B;
//byte configLedsModel = LEDS_MODEL_SK6812;

#define EEPROM_SIZE 10 // 10 bytes

// do not change!
#define MEMORY_VERSION 0
#define MEMORY_LEDS_MODEL 1 // 0 - WS2812b, 1 - SK6812 (1 byte)
#define MEMORY_SERVO_CLOSED 2 // integer (2 bytes)
#define MEMORY_SERVO_OPEN 4 // integer (2 bytes)

///////////// REMOTE CONNECTION

const char* ssid = "Tulipherd";
const char* password = "packofflowers"; // must be at least 8 characters

#define REMOTE_STATE_NOTHING 0
#define REMOTE_STATE_INIT 1
#define REMOTE_STATE_DISCONNECTED 2
#define REMOTE_STATE_CONNECTING 3
#define REMOTE_STATE_CONNECTED 4
#define REMOTE_STATE_AP 5

#define CHECK_CONNECTION_SECONDS 5

byte remoteState = REMOTE_STATE_NOTHING;
bool remoteActive = false;
unsigned int reconnectTimer = CHECK_CONNECTION_SECONDS;

#define PACKET_DATA_SIZE 4
typedef struct CommandData {
  byte blossomOpenness; // 0-100
  byte red; // 0-255
  byte green; // 0-255
  byte blue; // 0-255
};

typedef union CommandPacket {
  CommandData data;
  uint8_t packet[PACKET_DATA_SIZE];
};

#define UDP_PORT 1402

AsyncUDP udp;

///////////// POWER MODE

// tuned for 1600mAh LIPO battery
#define POWER_LOW_ENTER_THRESHOLD 0 // enter state when voltage drop below this threshold (3.55)
#define POWER_LOW_LEAVE_THRESHOLD 0 // leave state when voltage rise above this threshold (3.6)
#define POWER_DEAD_THRESHOLD 3.4

///////////// STATE OF FLOWER

#define MODE_INIT 0
#define MODE_STANDBY 1
#define MODE_BLOOM 3
#define MODE_BLOOMING 4
#define MODE_BLOOMED 5
#define MODE_CLOSE 6
#define MODE_CLOSING 7
#define MODE_CLOSED 8
#define MODE_FADE 9
#define MODE_FADING 10
#define MODE_FADED 11

#define MODE_BATTERYDEAD 20
#define MODE_SHUTDOWN 21
#define MODE_SHUTTINGDOWN 22

byte mode = MODE_INIT;

long changeModeTime = 0;
byte changeModeTo;

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

Flower flower;
long everySecondTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Tulip INITIALIZING");
  configure();

  // after wake up setup
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeup_reason) {
    Serial.println("Waking up after Deep Sleep");
    changeMode(MODE_BLOOM);
  }

  // init hardware
  WiFi.mode(WIFI_OFF);
  btStop();
  flower.init(configLedsModel);
  flower.readBatteryVoltage(); // calibrate the ADC
  flower.onLeafTouch(onLeafTouch);
  delay(100); // wait for init

  // check if there is enough power to run
  bool isBatteryDead = false;
  if (!flower.isUSBPowered()) {
    float voltage = flower.readBatteryVoltage();
    if (voltage < POWER_DEAD_THRESHOLD) {
      delay(500);
      voltage = flower.readBatteryVoltage(); // re-verify the voltage after .5s
      isBatteryDead = voltage < POWER_DEAD_THRESHOLD;
    }
  }

  if (isBatteryDead) {
    // battery is dead, do not wake up, shutdown after a status color
    Serial.println("Battery is dead, shutting down");
    changeMode(MODE_BATTERYDEAD);
    planChangeMode(MODE_SHUTDOWN, BATTERY_DEAD_WARNING_DURATION);
    flower.setLowPowerMode(true);
    flower.setColor(colorRed, FlowerColorMode::PULSE, 1000);
  }
  else {
    // normal operation
    flower.initServo(configServoClosed, configServoOpen);
    flower.setPetalsOpenLevel(0, 100);
  }

  everySecondTime = millis(); // TODO millis overflow
}

void loop() {
  flower.update();

  // timers
  long now = millis();
  if (everySecondTime > 0 && everySecondTime < now) {
    everySecondTime = now + 1000;
    everySecond();
  }
  if (changeModeTime > 0 && changeModeTime < now) {
    changeModeTime = 0;
    changeMode(changeModeTo);
  }

  // autonomous mode
  switch (mode) {
    case MODE_INIT:
      if (flower.isIdle()) {
        changeMode(MODE_STANDBY);
        Serial.println("Tulip READY");

        if (deepSleepEnabled) {
          planChangeMode(MODE_SHUTDOWN, DEEP_SLEEP_INACTIVITY_TIMEOUT);
        }
      }
      break;

    // faded -> bloomed
    case MODE_BLOOM:
      flower.setColor(nextRandomColor(), FlowerColorMode::TRANSITION, 5000);
      flower.setPetalsOpenLevel(100, 5000);
      changeMode(MODE_BLOOMING);
      break;

    case MODE_BLOOMING:
      if (flower.isIdle()) {
        changeMode(MODE_BLOOMED);
      }
      break;

    // bloomed -> closed with color ON
    case MODE_CLOSE:
      flower.setPetalsOpenLevel(0, 5000);
      changeMode(MODE_CLOSING);
      break;

    case MODE_CLOSING:
      if (flower.isIdle()) {
        changeMode(MODE_CLOSED);
      }
      break;

    // closed -> faded
    case MODE_FADE:
      flower.setColor(colorBlack, FlowerColorMode::TRANSITION, 2500);
      flower.setPetalsOpenLevel(0, 5000);
      changeMode(MODE_FADING);
      break;

    case MODE_FADING:
      if (flower.isIdle()) {
        changeMode(MODE_STANDBY);
        if (deepSleepEnabled) {
          planChangeMode(MODE_SHUTDOWN, DEEP_SLEEP_INACTIVITY_TIMEOUT);
        }
      }
      break;

    // shutdown to protect the flower
    case MODE_SHUTDOWN:
      flower.setColor(colorBlack, FlowerColorMode::TRANSITION, 500);
      flower.setPetalsOpenLevel(0, 5000);
      changeMode(MODE_SHUTTINGDOWN);
      break;

    case MODE_SHUTTINGDOWN:
      if (flower.isIdle()) {
        enterDeepSleep();
      }
      break;
  }

  // save some power when flower is idle
  if (flower.isIdle()) {
    delay(10);
  }
}

void everySecond() {
  //broadcastMasterState();
  //reconnectTimer--;
  flower.acty();
  powerWatchDog();
}

void planChangeMode(byte newMode, long timeout) {
  changeModeTo = newMode;
  changeModeTime = millis() + timeout;
  Serial.print("Planned change mode: ");
  Serial.print(newMode);
  Serial.print(" in ");
  Serial.println(timeout);
}

void changeMode(byte newMode) {
  if (mode != newMode) {
    mode = newMode;
    Serial.print("Change mode: ");
    Serial.println(newMode);
  }
  changeModeTime = 0;
}

void onLeafTouch() {
  Serial.println("Touched");
  if (mode == MODE_STANDBY) {
    changeMode(MODE_BLOOM);
  }
  else if (mode == MODE_BLOOMED) {
    changeMode(MODE_CLOSE);
  }
  else if (mode == MODE_CLOSED) {
    changeMode(MODE_FADE);
  }
}

void powerWatchDog() {
  if (mode == MODE_BATTERYDEAD) {
    return;
  }

  if (flower.isUSBPowered()) {
    flower.setLowPowerMode(false);
    return;
  }

  float voltage = flower.readBatteryVoltage();
  if (voltage < POWER_DEAD_THRESHOLD) {
    Serial.print("Shutting down, battery is dead (");
    Serial.print(voltage);
    Serial.println("V)");
    changeMode(MODE_SHUTDOWN);
  }
  else if (!flower.isLowPowerMode() && voltage < POWER_LOW_ENTER_THRESHOLD) {
    Serial.print("Entering low power mode (");
    Serial.print(voltage);
    Serial.println("V)");
    flower.setLowPowerMode(true);
  }
  else if (flower.isLowPowerMode() && voltage >= POWER_LOW_LEAVE_THRESHOLD) {
    Serial.print("Leaving low power mode (");
    Serial.print(voltage);
    Serial.println("V)");
    flower.setLowPowerMode(false);
  }
}

void enterDeepSleep() {
  esp_sleep_enable_touchpad_wakeup();
  Serial.println("Going to sleep now");
  WiFi.mode(WIFI_OFF);
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

    flower.setPetalsOpenLevel(command.blossomOpenness, 500); // TODO some default speed
    flower.setColor(RgbColor(command.red, command.green, command.blue), FlowerColorMode::TRANSITION, 500); // TODO some default speed
  }
}

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
