#include "automaton.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "Automaton";
#endif

#define STATE_STANDBY 0
#define STATE_BLOOMING 1
#define STATE_RUNNING 2
#define STATE_RAINBOW 11
#define STATE_CANDLE 12
#define STATE_REMOTE_INIT 99

Automaton::Automaton(Remote *remote, Floower *floower, Config *config)
    : remote(remote), floower(floower), config(config) {
}

void Automaton::init() {
  changeState(STATE_STANDBY);
  floower->onLeafTouch([=](FloowerTouchEvent event){ onLeafTouch(event); });
  remote->onTakeOver([=]() { onRemoteTookOver(); }); // remote controller took over
  enabled = true;
}

void Automaton::suspend() {
  floower->transitionColorBrightness(0, config->speedMillis);
  floower->setPetalsOpenLevel(0, config->speedMillis);
  changeState(STATE_STANDBY);
  enabled = false;
}

void Automaton::update() {
  // nop
}

/**
 * Implemented behavior:
 * 
 * (Standby) - Touch -> (Open + Lit) - Touch -> (Closed + Lit) - Touch -> (Standby)
 * (Standby) - Long Touch -> (Closed + Rainbow) - Touch -> (Closed + Lit)
 * (Open + Lit) - Long Touch -> (Open + Rainbow) - Touch -> (Open + Lit)
 * (Standby) - Hold Touch -> (Connect to Remote) - Touch -> (Standby)
 */
void Automaton::onLeafTouch(FloowerTouchEvent event) {
  if (!enabled || !config->touchEnabled) {
    return;
  }
  switch (event) {
    case TOUCH_DOWN:
      if (state == STATE_STANDBY && !floower->arePetalsMoving() && !floower->isChangingColor()) {
        // light up instantly on touch
        HsbColor nextColor = nextRandomColor();
        floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
        changeState(STATE_BLOOMING);
      }
      else if (state == STATE_RAINBOW) {
        // stop rainbow animation
        floower->stopAnimation(true);
        changeState(STATE_RUNNING);
        disabledTouchUp = true;
      }
      else if (state == STATE_REMOTE_INIT) {
        // disable remote init
        remote->stopAdvertising();
        config->setRemoteOnStartup(false);
        floower->transitionColorBrightness(0, 500);
        changeState(STATE_STANDBY);
      }
      break;

    case TOUCH_UP:
      if (disabledTouchUp) {
        disabledTouchUp = false;
      }
      else if (!floower->arePetalsMoving() && !floower->isChangingColor()) {
        if (state == STATE_BLOOMING) {
          // open
          floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
          changeState(STATE_RUNNING);
        }
        else if (state == STATE_STANDBY) {
          // open + set color
          if (!floower->isLit()) {
            HsbColor nextColor = nextRandomColor();
            floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
          }
          floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
          changeState(STATE_RUNNING);
        }
        else if (state == STATE_RUNNING && floower->getPetalsOpenLevel() > 0) {
          // close
          floower->setPetalsOpenLevel(0, config->speedMillis);
        }
        else if (state == STATE_RUNNING) {
          // shutdown
          floower->transitionColorBrightness(0, config->speedMillis / 2);
          changeState(STATE_STANDBY);  
        }
      }
      else if (state == STATE_BLOOMING) {
        // bloooom
        floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
        changeState(STATE_RUNNING);
      }
      break;

    case TOUCH_LONG:
      if (config->rainbowEnabled) {
        floower->startAnimation(FloowerColorAnimation::RAINBOW);
        changeState(STATE_RAINBOW);
        disabledTouchUp = true;
      }
      break;

    case TOUCH_HOLD:
      if (config->bluetoothEnabled && floower->getPetalsOpenLevel() == 0 && (state == STATE_STANDBY || state == STATE_RAINBOW || state == STATE_BLOOMING)) { // init remote when Floower closed (or rainbow started)
        floower->flashColor(colorBlue.H, colorBlue.S, 1000);
        remote->init();
        remote->startAdvertising();
        changeState(STATE_REMOTE_INIT);
        disabledTouchUp = true;
      }
      break;
  }
}

void Automaton::onRemoteTookOver() {
  changeState(STATE_RUNNING);
}

void Automaton::changeState(uint8_t newState) {
  if (state != newState) {
    state = newState;
    ESP_LOGD(LOG_TAG, "Changed state to %d", newState);
  }
}

HsbColor Automaton::nextRandomColor() {
  if (colorsUsed > 0) {
    unsigned long maxColors = pow(2, config->colorSchemeSize) - 1;
    if (maxColors == colorsUsed) {
      colorsUsed = 0; // all colors used, reset
    }
  }

  uint8_t colorIndex;
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
