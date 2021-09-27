#include "tmc2300.h"

TMC2300::TMC2300(Stream *serialPort, float RSense, uint8_t uartAddress) :
    serialPort(serialPort), RSense(RSense), uartAddress(uartAddress) {
    //defaults();
}

uint8_t TMC2300::testConnection() {
    uint32_t drv_status = read(REG_DRV_STATUS::address);
    switch (drv_status) {
        case 0xFFFFFFFF: return 1;
        case 0: return 2;
        default: return 0;
    }
}

REG_GCONF TMC2300::readGConfReg() {
    REG_GCONF gconf;
    gconf.sr = read(REG_GCONF::address);
    return gconf;
}

REG_GSTAT TMC2300::readGStat() {
    REG_GSTAT gstat;
    gstat.sr = read(REG_GSTAT::address);
    return gstat;
}

REG_IOIN TMC2300::readIontReg() {
    REG_IOIN iont;
    iont.sr = read(REG_IOIN::address);
    return iont;
}

void TMC2300::writeIontReg(REG_IOIN ioin) {
    write(REG_IOIN::address, ioin.sr);
}

void TMC2300::writeIholdIrunReg(REG_IHOLD_IRUN iholdIrun) {
    write(REG_IHOLD_IRUN::address, iholdIrun.sr);
}

REG_DRV_STATUS TMC2300::readDrvStatusReg() {
    REG_DRV_STATUS drvstatus;
    drvstatus.sr = read(REG_DRV_STATUS::address);
    return drvstatus;
}

void TMC2300::writeDrvStatusReg(REG_DRV_STATUS drvStatus) {
    write(REG_DRV_STATUS::address, drvStatus.sr);
}

REG_CHOPCONF TMC2300::readChopconf() {
    REG_CHOPCONF chopconf;
    chopconf.sr = read(REG_CHOPCONF::address);
    return chopconf;
}

void TMC2300::writeChopconfReg(REG_CHOPCONF chopconf) {
    write(REG_CHOPCONF::address, chopconf.sr);
}

void TMC2300::writeTCoolThrs(uint32_t tCoolThrs) {
    write(REG_TCOOLTHRS_ADDRESS, tCoolThrs);
}

uint8_t TMC2300::readSGValue() {
    return read(REG_SG_VALUE_ADDRESS);
}

void TMC2300::writeSGThrs(uint32_t sgThrs) {
    write(REG_SGTHRS_ADDRESS, sgThrs);
}

uint32_t TMC2300::read(uint8_t regAddr) {
    constexpr uint8_t len = 3;
    regAddr |= TMC_READ;

    uint8_t datagram[] = {TMC2300_SYNC, uartAddress, regAddr, 0x00};
    datagram[len] = calcCRC(datagram, len);
    uint64_t out = 0x00000000UL;

    for (uint8_t i = 0; i < maxRetries; i++) {
        out = sendDatagram(datagram, len, abortWindow);
        delay(replyDelay);

        CRCerror = false;
        uint8_t outDatagram[] = {
            static_cast<uint8_t>(out>>56),
            static_cast<uint8_t>(out>>48),
            static_cast<uint8_t>(out>>40),
            static_cast<uint8_t>(out>>32),
            static_cast<uint8_t>(out>>24),
            static_cast<uint8_t>(out>>16),
            static_cast<uint8_t>(out>> 8),
            static_cast<uint8_t>(out>> 0)
        };
        uint8_t crc = calcCRC(outDatagram, 7);
        if ((crc != static_cast<uint8_t>(out)) || crc == 0 ) {
            CRCerror = true;
            out = 0;
        } else {
            break;
        }
    }

    return out>>8;
}

void TMC2300::write(uint8_t regAddr, uint32_t regVal) {
    uint8_t len = 7;
    regAddr |= TMC_WRITE;

    uint8_t datagram[] = {TMC2300_SYNC, uartAddress, regAddr, (uint8_t)(regVal>>24), (uint8_t)(regVal>>16), (uint8_t)(regVal>>8), (uint8_t)(regVal>>0), 0x00};
    datagram[len] = calcCRC(datagram, len);

    for (uint8_t i = 0; i <= len; i++) {
        bytesWritten += serialPort->write(datagram[i]);
    }

    delay(replyDelay);
}

uint8_t TMC2300::calcCRC(uint8_t datagram[], uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t currentByte = datagram[i];
        for (uint8_t j = 0; j < 8; j++) {
            if ((crc >> 7) ^ (currentByte & 0x01)) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = (crc << 1);
            }
            crc &= 0xff;
            currentByte = currentByte >> 1;
        }
    }
    return crc;
}

uint64_t TMC2300::sendDatagram(uint8_t datagram[], const uint8_t len, uint16_t timeout) {
    while (serialPort->available() > 0) { // flush
        serialPort->read();
    }

    for (int i = 0; i <= len; i++) {
        serialPort->write(datagram[i]);
    }

    delay(replyDelay);

    // scan for the rx frame and read it
    uint32_t ms = millis();
    uint32_t sync_target = (static_cast<uint32_t>(datagram[0])<<16) | 0xFF00 | datagram[2];
    uint32_t sync = 0;

    do {
        uint32_t ms2 = millis();
        if (ms2 != ms) {
            // 1ms tick
            ms = ms2;
            timeout--;
        }
        if (!timeout) {
            return 0;
        }

        int16_t res = serialPort->read();
        if (res < 0) {
            continue;
        }

        sync <<= 8;
        sync |= res & 0xFF;
        sync &= 0xFFFFFF;

    } while (sync != sync_target);

    uint64_t out = sync;
    ms = millis();
    timeout = this->abortWindow;

    for(uint8_t i=0; i<5;) {
        uint32_t ms2 = millis();
        if (ms2 != ms) {
            // 1ms tick
            ms = ms2;
          timeout--;
        }
        if (!timeout) {
            return 0;
        }

        int16_t res = serialPort->read();
        if (res < 0) {
            continue;
        }

        out <<= 8;
        out |= res & 0xFF;
        i++;
    }

    while (serialPort->available() > 0) {
        serialPort->read(); // flush
    }

    return out;
}
