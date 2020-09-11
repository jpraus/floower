#ifndef REMOTE_H
#define REMOTE_H

#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "floower.h"

#define PACKET_DATA_SIZE 4

typedef struct StatePacketData {
  byte petalsOpenLevel; // 0-100%
  byte R; // 0-255
  byte G; // 0-255
  byte B; // 0-255

  RgbColor getColor() {
    return RgbColor(R, G, B);
  }
};

typedef union StatePacket {
  StatePacketData data;
  uint8_t bytes[PACKET_DATA_SIZE];
};

class Remote {
  public:
    Remote(Floower *floower);
    void init();
    void update();

  private:
    Floower *floower;
    BLEServer *server = NULL;
    //BLECharacteristic * pTxCharacteristic;

    bool deviceConnected = false;

    BLECharacteristic* createROCharacteristics(BLEService *service, const char *uuid, const char *value);
    BLECharacteristic* createROCharacteristics(BLEService *service, const char *uuid, int value);

    // BLE state characteristics callback
    class StateCharacteristicsCallbacks : public BLECharacteristicCallbacks {
      public:
        StateCharacteristicsCallbacks(Remote* remote) : remote(remote) {};
      private:
        Remote* remote ;
        void onWrite(BLECharacteristic *characteristic);
    };

    // BLE state characteristics callback
    class ColorRGBCharacteristicsCallbacks : public BLECharacteristicCallbacks {
      public:
        ColorRGBCharacteristicsCallbacks(Remote* remote) : remote(remote) {};
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
