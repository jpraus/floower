#ifndef REMOTE_H
#define REMOTE_H

#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"
#include "floower.h"

#define STATE_PACKET_SIZE 4

typedef struct StatePacketData {
  byte petalsOpenLevel; // 0-100%, read-write
  byte R; // 0-255, read-write
  byte G; // 0-255, read-write
  byte B; // 0-255, read-write

  RgbColor getColor() {
    return RgbColor(R, G, B);
  }
};

typedef union StatePacket {
  StatePacketData data;
  uint8_t bytes[STATE_PACKET_SIZE];
};

#define STATE_CHANGE_PACKET_SIZE 6

// TODO: define
#define STATE_TRANSITION_MODE_COLOR B01
#define STATE_TRANSITION_MODE_PETALS B10
#define STATE_TRANSITION_MODE_COLOR_PETALS B11

typedef struct StateChangePacketData : StatePacketData {
  byte duration; // 100 of milliseconds
  byte mode; // transition mode
};

typedef union StateChangePacket {
  StateChangePacketData data;
  uint8_t bytes[STATE_CHANGE_PACKET_SIZE];
};

class Remote {
  public:
    Remote(Floower *floower, Config *config);
    void init();
    void update();
    void setState(uint8_t petalsOpenLevel, RgbColor color);
    void setBatteryLevel(uint8_t level, bool charging);

  private:
    Floower *floower;
    Config *config;
    BLEServer *server = NULL;
    BLEService *floowerService = NULL;
    BLEService *batteryService = NULL;

    bool deviceConnected = false;
    bool initialized = false;

    BLECharacteristic* createROCharacteristics(BLEService *service, const char *uuid, const char *value);

    // BLE state characteristics callback
    class StateChangeCharacteristicsCallbacks : public BLECharacteristicCallbacks {
      public:
        StateChangeCharacteristicsCallbacks(Remote* remote) : remote(remote) {};
      private:
        Remote* remote ;
        void onWrite(BLECharacteristic *characteristic);
    };

    // BLE name characteristics callback
    class NameCharacteristicsCallbacks : public BLECharacteristicCallbacks {
      public:
        NameCharacteristicsCallbacks(Remote* remote) : remote(remote) {};
      private:
        Remote* remote ;
        void onWrite(BLECharacteristic *characteristic);
    };

    // BLE color scheme characteristics callback
    class ColorsSchemeCharacteristicsCallbacks : public BLECharacteristicCallbacks {
      public:
        ColorsSchemeCharacteristicsCallbacks(Remote* remote) : remote(remote) {};
      private:
        Remote* remote ;
        void onWrite(BLECharacteristic *characteristic);
    };

    // BLE touch treshold characteristics callback
    class TouchTresholdCharacteristicsCallbacks : public BLECharacteristicCallbacks {
      public:
        TouchTresholdCharacteristicsCallbacks(Remote* remote) : remote(remote) {};
      private:
        Remote* remote ;
        void onWrite(BLECharacteristic *characteristic);
    };

    // BLE server callbacks impl
    class ServerCallbacks : public BLEServerCallbacks {
      public:
        ServerCallbacks(Remote* remote) : remote(remote) {};
      private:
        Remote* remote ;
        void onConnect(BLEServer* server);
        void onDisconnect(BLEServer* server);
    };
};

#endif
