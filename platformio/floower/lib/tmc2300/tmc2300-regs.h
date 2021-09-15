#pragma once
#pragma pack(push, 1)

struct REG_IOIN {
    constexpr static uint8_t address = 0x06;
    union {
        uint32_t sr;
        struct {
            bool en : 1;
            bool nstdby : 1;
            bool ad0 : 1;
            bool ad1 : 1;
            bool diag : 1;
            bool stepper : 1;
            bool pdn_uart : 1;
            bool mode : 1;
            bool step : 1;
            bool dir : 1;
            bool comp_a1a2 : 1;
            bool comp_b1b2 : 1;
            uint16_t : 12;
            uint8_t version : 8;
        };
    };
};


struct REG_GCONF {
    constexpr static uint8_t address = 0x00;
    union {
        uint32_t sr : 8;
        struct {
            bool : 1;
            bool extcap : 1;
            bool : 1;
            bool shaft : 1;
            bool diag_index : 1;
            bool diag_step : 1;
            bool multistep_filt : 1;
            bool test_mode : 1;
        };
    };
};

struct REG_GSTAT {
    constexpr static uint8_t address = 0x01;
    union {
        uint32_t sr : 3;
        struct {
            bool reset : 1;
            bool drv_err : 1;
            bool u3v5 : 1;
        };
    };
};

struct REG_IHOLD_IRUN {
    constexpr static uint8_t address = 0x10;
    union {
        uint32_t sr : 14;
        struct {
            uint8_t ihold : 5;
            uint8_t: 3;
            uint8_t irun : 5;
            uint8_t: 3;
            uint8_t iholddelay : 4;
        };
    };
};

struct REG_DRV_STATUS {
    constexpr static uint8_t address = 0x6F;
    union {
        uint32_t sr;
        struct {
            bool otpw : 1;
            bool ot : 1;
            bool s2ga : 1;
            bool s2gb : 1;
            bool s2vsa : 1;
            bool s2vsb : 1;
            bool ola : 1;
            bool olb : 1;
            bool t120 : 1;
            bool t150 : 1;
            uint8_t: 2;
            uint8_t: 4;
            uint8_t cs_actual : 5;
            uint8_t: 3;
            uint8_t: 7;
            bool stst : 1;
        };
    };
};

struct REG_CHOPCONF {
    constexpr static uint8_t address = 0x6C;
    union {
        uint32_t sr;
        struct {
            bool enabledrv : 1;
            uint16_t : 14;
            uint8_t tbl : 2;
            uint8_t : 7;
            uint8_t mres : 4;
            bool intpol : 1;
            bool dedge : 1;
            bool diss2g : 1;
            bool diss2vs : 1;
        };
    };

    void setMicrosteps(uint16_t ms) {
        switch(ms) {
            case 256: mres = 0; break;
            case 128: mres = 1; break;
            case  64: mres = 2; break;
            case  32: mres = 3; break;
            case  16: mres = 4; break;
            case   8: mres = 5; break;
            case   4: mres = 6; break;
            case   2: mres = 7; break;
            case   0: mres = 8; break;
            default: break;
        }
    }
    
    uint16_t getMicrosteps() {
        switch(mres) {
            case 0: return 256;
            case 1: return 128;
            case 2: return  64;
            case 3: return  32;
            case 4: return  16;
            case 5: return   8;
            case 6: return   4;
            case 7: return   2;
            case 8: return   0;
        }
        return 0;
    }
};

const uint8_t REG_TCOOLTHRS_ADDRESS = 0x14;
const uint8_t REG_SGTHRS_ADDRESS = 0x40;
const uint8_t REG_SG_VALUE_ADDRESS = 0x41;
const uint8_t REG_COOLCONF_ADDRESS = 0x42;

#pragma pack(pop)
