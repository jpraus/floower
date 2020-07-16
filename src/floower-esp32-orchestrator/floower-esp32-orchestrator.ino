#include <WiFi.h>
#include <AsyncUDP.h>

///////////// REMOTE CONNECTION

const char* ssid = "Tulipherd";
const char* password = "packofflowers"; // must be at least 8 characters

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

// GESTURE SENSORS

#define TRIGGER_PIN 4
#define ECHO_PIN 35

#define DISTANCE_OUT_OF_RANGE 99999

// STATE

int blossomOpenness = 0;

#define ACTY_LED_PIN 2

int everySecondTimer = 0;
unsigned long deltaMsRef = 0;

// CODE

void setup() {
  Serial.begin(115200);

  // ultrasonic sensors
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT_PULLDOWN);

  // init
  deltaMsRef = millis();
  everySecondTimer = 1000;

  Serial.println("Tulip orchestrator");
  initAP();
}

int counter = 0;
byte speed = 5;

void loop() {
  int deltaMs = getDeltaMs();

  long distance = readDistance();
  if (distance != DISTANCE_OUT_OF_RANGE) { // 400-2000
    // convert distance to percentage of openness
    if (distance > 1600) {
      blossomOpenness = 0;
    }
    else if (distance > 1300) {
      blossomOpenness = 20;
    }
    else if (distance > 1000) {
      blossomOpenness = 40;
    }
    else if (distance > 700) {
      blossomOpenness = 60;
    }
    else if (distance > 400) {
      blossomOpenness = 80;
    }
    else {
      blossomOpenness = 100;
    }
    Serial.println(blossomOpenness);
  }
  else {
    blossomOpenness = 0;
  }

  everySecondTimer -= deltaMs;
  if (everySecondTimer <= 0) {
    everySecondTimer += 100;
    digitalWrite(ACTY_LED_PIN, HIGH);
    broadcastMasterState();
  }
  else {
    digitalWrite(ACTY_LED_PIN, LOW);
  }

  delay(100);
}

long readDistance() {
  // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);
 
  // Read the signal from the sensor: a HIGH pulse whose
  // duration is the time (in microseconds) from the sending
  // of the ping to the reception of its echo off of an object.
  long duration = pulseIn(ECHO_PIN, HIGH);
 
  Serial.print("Ultrasonic: ");
  Serial.println(duration);

  if (duration > 2000) {
    return DISTANCE_OUT_OF_RANGE;
  }
  return duration;
}

// remote control

void initAP() {
  // server
  Serial.print("Configuring access point ... ");
  WiFi.softAP(ssid, password, 1, 0, 8);
  Serial.println("OK");

  udp.connect(WiFi.softAPIP(), UDP_PORT);
  Serial.print("UDP Server started on IP: ");
  Serial.println(WiFi.softAPIP());
}

void broadcastMasterState() {
  CommandData command;
  command.blossomOpenness = blossomOpenness;
  command.red = blossomOpenness;
  command.green = 0;
  command.blue = blossomOpenness;
  broadcastRemoteCommand(command);
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
