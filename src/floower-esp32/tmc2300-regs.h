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

struct REG_IHOLD_IRUN {
  constexpr static uint8_t address = 0x10;
  union {
    uint32_t sr : 14;
    struct {
      uint8_t ihold : 5;
      uint8_t irun : 5;
      uint8_t iholddelay : 4;
    };
  };

  void setRmsCurrent(uint16_t mA, float RSense, float holdMultiplier = 0.5) {
    uint8_t CS = 32.0 * 1.41421 * mA / 1000.0 * (RSense + 0.02) / 0.325 - 1;

    // if Current Scale is too low, turn on high sensitivity R_sense and calculate again
    if (CS < 16) {
      vsense(true);
      CS = 32.0 * 1.41421 * mA / 1000.0 * (RSense + 0.02) / 0.180 - 1;
    }
    else { // If CS >= 16, turn off high_sense_r
      vsense(false);
    }
    if (CS > 31) {
      CS = 31;
    }
    irun = CS;
    ihold = CS * holdMultiplier;
  }
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

#pragma pack(pop)
