#include "behavior/BloomingBehavior.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "BloomingBehavior";
#endif

#define STATE_BLOOM_LIGHT 128
#define STATE_BLOOM_OPENING 129
#define STATE_BLOOMED 130
#define STATE_BLOOMED_PICKER 131
#define STATE_BLOOM_CLOSING 132
#define STATE_LIGHT 133
#define STATE_LIGHT_PICKER 134
#define STATE_GOING_OFF 135

BloomingBehavior::BloomingBehavior(Config *config, Floower *floower, Remote *remote) 
        : SmartPowerBehavior(config, floower, remote) {
}

void BloomingBehavior::update() {
    SmartPowerBehavior::update();

    if (state != STATE_STANDBY) {
        changeStateIfIdle(STATE_BLOOM_OPENING, STATE_BLOOMED);
        changeStateIfIdle(STATE_BLOOM_CLOSING, STATE_LIGHT);
        changeStateIfIdle(STATE_GOING_OFF, STATE_STANDBY);
    }
}

bool BloomingBehavior::onLeafTouch(FloowerTouchEvent event) {
    if (SmartPowerBehavior::onLeafTouch(event)) {
        return true;
    }
    else if (event == TOUCH_DOWN) {
        Serial.println(state);
        Serial.println(STATE_STANDBY);
        if (state == STATE_STANDBY) {
            // light up instantly on touch
            HsbColor nextColor = nextRandomColor();
            floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
            changeState(STATE_BLOOM_LIGHT);
            return true;
        }
        else if (state == STATE_BLOOMED_PICKER) {
            // stop color picker animation
            floower->stopAnimation(true);
            preventTouchUp = true;
            changeState(STATE_BLOOMED);
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
        if (preventTouchUp) {
            preventTouchUp = false;
        }
        else {
            if (state == STATE_STANDBY) {
                // light + open
                HsbColor nextColor = nextRandomColor();
                floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
                floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
                changeState(STATE_BLOOM_OPENING);
                return true;
            }
            else if (state == STATE_BLOOM_LIGHT) {
                // open
                floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
                changeState(STATE_BLOOM_OPENING);
                return true;
            }
            else if (state == STATE_BLOOMED) {
                // close
                floower->setPetalsOpenLevel(0, config->speedMillis);
                changeState(STATE_BLOOM_CLOSING);
                return true;
            }
            else if (state == STATE_LIGHT) {
                // shutdown
                floower->transitionColorBrightness(0, config->speedMillis / 2);
                changeState(STATE_GOING_OFF);
                return true;
            }
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
            else if (state == STATE_BLOOMED) {
                floower->startAnimation(FloowerColorAnimation::RAINBOW);
                preventTouchUp = true;
                changeState(STATE_BLOOMED_PICKER);
                return true;
            }
        }
    }
    return false;
}

HsbColor BloomingBehavior::nextRandomColor() {
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