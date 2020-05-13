#include <EEPROM.h>
#include <NeoPixelBus.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include "flower.h"

bool remoteMode = false;
bool remoteMaster = false;
bool deepSleepEnabled = false;

///////////// PERSISTENT CONFIGURATION

boolean writeConfiguration = false;

const byte CONFIG_VERSION = 1;
int configServoClosed = 650;
int configServoOpen = configServoClosed + 750;
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

#define CHECK_CONNECTION_EVERY_MS 5000

byte remoteState = REMOTE_STATE_NOTHING;
bool remoteActive = false;
int reconnectTimer = CHECK_CONNECTION_EVERY_MS;

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

///////////// TOUCH

#define TOUCH_SENSOR_PIN 4
#define TOUCH_TRESHOLD 45 // 45

///////////// STATE OF FLOWER

#define MODE_INIT 0
#define MODE_SLEEPING 1
#define MODE_BLOOM 3
#define MODE_BLOOMING 4
#define MODE_BLOOMED 5
#define MODE_CLOSE 6
#define MODE_CLOSING 7
#define MODE_CLOSED 8
#define MODE_FADE 9
#define MODE_FADING 10
#define MODE_FADED 11
#define MODE_FALLINGASLEEP 12

#define ACTY_LED_PIN 2

byte mode = MODE_INIT;
byte statusColor = 0; // TODO: prettier

RgbColor colors[] = {
  colorWhite,
  colorYellow,
  colorOrange,
  colorRed,
  colorPink,
  colorPurple,
  colorBlue
};
byte colorsCount = 7;

///////////// POWER MANAGEMENT

#define BATTERY_ANALOG_IN 36 // VP

///////////// CODE

Flower flower;

int everySecondTimer = 0;
unsigned long deltaMsRef = 0;
int initTimeout = 0; // give the electronics some time to warm up before starting WiFi

void setup() {
  Serial.begin(115200);
  //Serial.setDebugOutput(0);

  pinMode(ACTY_LED_PIN, OUTPUT);
  digitalWrite(ACTY_LED_PIN, HIGH);

  configure();

  // after wake up setup
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeup_reason) {
    //servoPosition = configServoClosed; // startup from sleep - tulip was closed
    changeMode(MODE_BLOOM);
  }
  else {
    //servoPosition = configServoClosed + 100; // starting from unknown position - let it open a little bit to make sure it won't crash
    initTimeout = 2000; // 2s
  }

  // configure ADC
  analogReadResolution(12); // se0t 12bit resolution (0-4095)
  analogSetAttenuation(ADC_11db); // set AREF to be 3.6V
  analogSetCycles(8); // num of cycles per sample, 8 is default optimal
  analogSetSamples(1); // num of samples

  // touch
  touchAttachInterrupt(TOUCH_SENSOR_PIN, onLeafTouch, TOUCH_TRESHOLD);

  // init
  if (remoteMode || remoteMaster) {
    statusColor = 1;
  }
  else {
    WiFi.mode(WIFI_OFF);
    btStop();
  }

  deltaMsRef = millis();
  everySecondTimer = 1000;

  // do blossom calibration
  Serial.println("Tulip INITIALIZING");

  flower.init(configServoClosed, configServoOpen, configLedsModel);
  //flower.setPetalsOpenLevel(100, 5000);
}

int counter = 0;

void loop() {
  flower.update();
  
  int deltaMs = getDeltaMs();

  if (remoteMode || remoteMaster) {
    remoteControl();
  }

  if (remoteMode && remoteActive) {
    // remote control is active
  }
  else {
    // autonomous mode is active
    switch (mode) {
      case MODE_INIT:
        if (initTimeout > 0) {
          initTimeout -= deltaMs;
        }
        else if (flower.isIdle()) {
          if (remoteMaster) {
            // init AP here
          }
          changeMode(MODE_SLEEPING);
          remoteState = REMOTE_STATE_INIT;
          Serial.println("Tulip READY");
        }
        break;

      // faded -> bloomed
      case MODE_BLOOM:
        flower.setColor(colors[random(0, colorsCount)], 5000);
        flower.setPetalsOpenLevel(100, 5000);
        changeMode(MODE_BLOOMING);
        break;
  
      case MODE_BLOOMING:
        if (flower.isIdle()) {
          changeMode(MODE_BLOOMED);
        }
        break;

      // bloomed -> closed with color on
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
        flower.setColor(colorBlack, 5000);
        flower.setPetalsOpenLevel(0, 5000);
        changeMode(MODE_FADING);
        break;
  
      case MODE_FADING:
        if (flower.isIdle()) {
          changeMode(MODE_FALLINGASLEEP);
        }
        break;
  
      case MODE_FALLINGASLEEP:
        if (flower.isIdle()) {
          changeMode(MODE_SLEEPING);
          //setPixelsPowerOn(false);

          if (deepSleepEnabled) {
            esp_sleep_enable_touchpad_wakeup();
            Serial.println("Going to sleep now");
            WiFi.mode(WIFI_OFF);
            btStop();
            esp_deep_sleep_start();
          }
        }
        break;
    }
  }

  everySecondTimer -= deltaMs;
  reconnectTimer -= deltaMs;

  if (everySecondTimer <= 0) {
    everySecondTimer += 1000;
    digitalWrite(ACTY_LED_PIN, HIGH);
    broadcastMasterState();

    float batteryVoltage = readBatteryVoltage();
  }
  else {
    digitalWrite(ACTY_LED_PIN, LOW);
  }

  //delay(1);
}

void changeMode(byte newMode) {
  if (mode != newMode) {
    mode = newMode;
    counter = 0;
    Serial.print("Change mode: ");
    Serial.println(newMode);
  }
}

void onLeafTouch() {
  Serial.println("Touched");
  if (mode == MODE_SLEEPING) {
    changeMode(MODE_BLOOM);
  }
  else if (mode == MODE_BLOOMED) {
    changeMode(MODE_CLOSE);
  }
  else if (mode == MODE_CLOSED) {
    changeMode(MODE_FADE);
  }
}

// remote control

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
        statusColor = 4; // AP ready
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
      statusColor = 2;
      break;

    case REMOTE_STATE_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");

        if (udp.listen(UDP_PORT)) {
          Serial.print("UDP Listening on IP: ");
          Serial.println(WiFi.localIP());
          udp.onPacket(udpPacketReceived);
        }

        statusColor = 3; // until first data packet
        remoteActive = true;
        remoteState = REMOTE_STATE_CONNECTED;
      }
      break;

    case REMOTE_STATE_CONNECTED:
      if (reconnectTimer <= 0) {
        reconnectTimer = CHECK_CONNECTION_EVERY_MS;
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

  if (statusColor > 0) {
    statusColor = 4; // first packet received
  }

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
    flower.setColor(RgbColor(command.red, command.green, command.blue), 500); // TODO some default speed
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

// utility functions

int getDeltaMs() {
  unsigned long time = millis();
  if (time < deltaMsRef) {
    deltaMsRef = time; // overflow of millis happen, we lost some millis but that does not matter for us, keep moving on
    return time;
  }
  int deltaMs = time - deltaMsRef;
  deltaMsRef += deltaMs;
  return deltaMs;
}

float readBatteryVoltage() {
  float reading = analogRead(BATTERY_ANALOG_IN); // 0-4095
  float voltage = reading / 4096.0 * 3.7 * 2; // Analog reference voltage is 3.6V, using 1:1 voltage divider (*2)

  Serial.print("Battery ");
  Serial.print(reading);
  Serial.print(" ");
  Serial.print(voltage);
  Serial.println("V");

  return voltage;
}
