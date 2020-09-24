#include "automaton.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Automaton";
#endif

#define STATE_STANDBY 0
#define STATE_OPEN_LIT 1
#define STATE_CLOSED_LIT 2

Automaton::Automaton(Floower *floower, Config *config)
    : floower(floower), config(config) {
}

void Automaton::init() {
  changeState(STATE_STANDBY);
  floower->onLeafTouch([=](FloowerTouchType touchType){ onLeafTouch(touchType); });
}

void Automaton::update() {
}

bool Automaton::canEnterDeepSleep() {
  return state == STATE_STANDBY && floower->isIdle();
}

void Automaton::changeState(uint8_t newState) {
  if (state != newState) {
    state = newState;
    ESP_LOGD(LOG_TAG, "Changed state to %d", newState);
  }
}

void Automaton::onLeafTouch(FloowerTouchType touchType) {
  switch (touchType) {
    case RELEASE:
      if (colorPickerOn) {
        floower->stopColorPicker();
        colorPickerOn = false;
      }
      else if (floower->isIdle()) {
        if (state == STATE_STANDBY) {
          // open + set color
          floower->setColor(nextRandomColor(), FloowerColorMode::TRANSITION, 5000);
          floower->setPetalsOpenLevel(100, 5000);
          changeState(STATE_OPEN_LIT);
        }
        else if (state == STATE_OPEN_LIT) {
          // close
          floower->setPetalsOpenLevel(0, 5000);
          changeState(STATE_CLOSED_LIT);
        }
        else if (state == STATE_CLOSED_LIT) {
          // shutdown
          floower->setColor(colorBlack, FloowerColorMode::TRANSITION, 2000);
          changeState(STATE_STANDBY);
        }
      }
      break;

    case HOLD:
      floower->startColorPicker();
      colorPickerOn = true;
      if (state == STATE_STANDBY) {
        changeState(STATE_CLOSED_LIT);
      }
      break;
  }
}

RgbColor Automaton::nextRandomColor() {
  if (colorsUsed > 0) {
    unsigned long maxColors = pow(2, config->colorSchemeSize) - 1;
    if (maxColors == colorsUsed) {
      colorsUsed = 0; // all colors used, reset
    }
  }

  byte colorIndex;
  long colorCode;
  int maxIterations = config->colorSchemeSize * 3;

  do {
    colorIndex = random(0, config->colorSchemeSize);
    colorCode = 1 << colorIndex;
    maxIterations--;
  } while ((colorsUsed & colorCode) > 0 && maxIterations > 0); // already used before all the rest colors

  colorsUsed += colorCode;
  return config->colorScheme[colorIndex];
}
