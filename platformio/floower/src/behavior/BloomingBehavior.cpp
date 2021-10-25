#include "behavior/BloomingBehavior.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "BloomingBehavior";
#endif

#define STATE_BLOOM_LIGHT 128
#define STATE_BLOOM_OPEN 129
#define STATE_BLOOM 130
#define STATE_BLOOM_PICKER 131
#define STATE_BLOOM_CLOSE 132
#define STATE_LIGHT 133
#define STATE_LIGHT_PICKER 134
#define STATE_FADE 135

BloomingBehavior::BloomingBehavior(Config *config, Floower *floower, BluetoothConnect *bluetoothConnect) 
        : SmartPowerBehavior(config, floower, bluetoothConnect) {
}

void BloomingBehavior::loop() {
    SmartPowerBehavior::loop();

    if (state != STATE_STANDBY) {
        changeStateIfIdle(STATE_BLOOM_OPEN, STATE_BLOOM);
        changeStateIfIdle(STATE_BLOOM_CLOSE, STATE_LIGHT);
        changeStateIfIdle(STATE_FADE, STATE_STANDBY);
    }
}

bool BloomingBehavior::onLeafTouch(FloowerTouchEvent event) {
    if (SmartPowerBehavior::onLeafTouch(event)) {
        return true;
    }
    else if (event == TOUCH_DOWN) {
        if (state == STATE_STANDBY) {
            // light up instantly on touch
            HsbColor nextColor = nextRandomColor();
            floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
            changeState(STATE_BLOOM_LIGHT);
            return true;
        }
        else if (state == STATE_REMOTE_CONTROL) {
            // floower is controlled by remote control, emulate this behavior state to react correctly
            if (floower->getPetalsOpenLevel() > 0) {
                if (!floower->isLit()) {
                    HsbColor nextColor = nextRandomColor();
                    floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
                }
                changeState(STATE_BLOOM);
            }
            else if (floower->isLit()) {
                changeState(STATE_LIGHT);
            }
        }
        else if (state == STATE_BLOOM_PICKER) {
            // stop color picker animation
            floower->stopAnimation(true);
            preventTouchUp = true;
            changeState(STATE_BLOOM);
            return true;
        }
        else if (state == STATE_LIGHT_PICKER) {
            // stop color picker animation
            floower->stopAnimation(true);
            preventTouchUp = true;
            changeState(STATE_LIGHT);
            return true;
        }
    }
    else if (event == TOUCH_UP) {
        if (state == STATE_STANDBY) {
            // light + open
            HsbColor nextColor = nextRandomColor();
            floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
            floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
            changeState(STATE_BLOOM_OPEN);
            return true;
        }
        else if (state == STATE_BLOOM_LIGHT) {
            // open
            floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
            changeState(STATE_BLOOM_OPEN);
            return true;
        }
        else if (state == STATE_BLOOM) {
            // close
            floower->setPetalsOpenLevel(0, config->speedMillis);
            changeState(STATE_BLOOM_CLOSE);
            return true;
        }
        else if (state == STATE_LIGHT) {
            // shutdown
            floower->transitionColorBrightness(0, config->speedMillis / 2);
            changeState(STATE_FADE);
            return true;
        }
    }
    else if (event == TOUCH_LONG) {
        if (config->colorPickerEnabled) {
            if (state == STATE_STANDBY || state == STATE_BLOOM_LIGHT || state == STATE_LIGHT) {
                floower->startAnimation(FloowerColorAnimation::RAINBOW);
                preventTouchUp = true;
                changeState(STATE_LIGHT_PICKER);
                return true;
            }
            else if (state == STATE_BLOOM) {
                floower->startAnimation(FloowerColorAnimation::RAINBOW);
                preventTouchUp = true;
                changeState(STATE_BLOOM_PICKER);
                return true;
            }
        }
    }
    return false;
}

bool BloomingBehavior::canInitializeBluetooth() {
    return SmartPowerBehavior::canInitializeBluetooth() || state == STATE_LIGHT_PICKER || state == STATE_BLOOM_LIGHT;
}

