#pragma once

#include <Arduino.h>
#include <Stream.h>
#include "tmc2300-regs.h"

class TMC2300 {
  public:
    TMC2300(Stream *serialPort, float RSense, uint8_t addr);
    uint8_t testConnection();

    REG_GCONF readGConfReg();
    REG_GSTAT readGStat();

    REG_IOIN readIontReg();
    void writeIontReg(REG_IOIN ioin);

    REG_IHOLD_IRUN readIholdIrunReg();
    void writeIholdIrunReg(REG_IHOLD_IRUN iholdIrun);

    REG_DRV_STATUS readDrvStatusReg();
    void writeDrvStatusReg(REG_DRV_STATUS drvStatus);

    REG_CHOPCONF readChopconf();
    void writeChopconfReg(REG_CHOPCONF chopconf);
    
  private:
    uint32_t read(uint8_t addr);
    void write(uint8_t regAddr, uint32_t regVal);
    uint8_t calcCRC(uint8_t datagram[], uint8_t len);
    uint64_t sendDatagram(uint8_t datagram[], const uint8_t len, uint16_t timeout);

    Stream *serialPort = nullptr;
    const float RSense;
    const uint8_t uartAddress;
    uint16_t bytesWritten = 0;
    bool CRCerror = false;

    static constexpr uint8_t TMC_READ = 0x00;
    static constexpr uint8_t TMC_WRITE = 0x80;
    static constexpr uint8_t TMC2300_SYNC = 0x05;
    static constexpr uint8_t replyDelay = 2;
    static constexpr uint8_t abortWindow = 5;
    static constexpr uint8_t maxRetries = 2;
};