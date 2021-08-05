#include "behavior/BloomingBehavior.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "BloomingBehavior";
#endif

#define STATE_STANDBY 0
#define STATE_BLOOMING 10
#define STATE_RUNNING 11
#define STATE_RAINBOW 12
#define STATE_CANDLE 13

BloomingBehavior::BloomingBehavior(Config *config, Floower *floower, Remote *remote)
    : config(config), floower(floower), remote(remote) {
}

bool BloomingBehavior::init() {
    state = STATE_STANDBY;
    return false;
}

bool BloomingBehavior::update() {
    return true;
}

void BloomingBehavior::suspend() {

}

void BloomingBehavior::resume() {

}

bool BloomingBehavior::onLeafTouch(FloowerTouchEvent event) {
    switch (event) {
        case TOUCH_DOWN:
            if (state == STATE_STANDBY && !floower->arePetalsMoving() && !floower->isChangingColor()) {
                // light up instantly on touch
                HsbColor nextColor = nextRandomColor();
                floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
                state = STATE_BLOOMING;
            }
            else if (state == STATE_RAINBOW) {
                // stop rainbow animation
                floower->stopAnimation(true);
                state = STATE_RUNNING;
                disabledTouchUp = true;
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
                    state = STATE_RUNNING;
                }
                else if (state == STATE_STANDBY) {
                    // open + set color
                    if (!floower->isLit()) {
                        HsbColor nextColor = nextRandomColor();
                        floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, config->speedMillis);
                    }
                    floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
                    state = STATE_RUNNING;
                }
                else if (state == STATE_RUNNING && floower->getPetalsOpenLevel() > 0) {
                    // close
                    floower->setPetalsOpenLevel(0, config->speedMillis);
                }
                else if (state == STATE_RUNNING) {
                    // shutdown
                    floower->transitionColorBrightness(0, config->speedMillis / 2);
                    state = STATE_STANDBY; 
                }
            }
            else if (state == STATE_BLOOMING) {
                // bloooom
                floower->setPetalsOpenLevel(config->personification.maxOpenLevel, config->speedMillis);
                state = STATE_RUNNING;
            }
            break;

        case TOUCH_LONG:
            if (config->rainbowEnabled) {
                floower->startAnimation(FloowerColorAnimation::RAINBOW);
                state = STATE_RAINBOW;
                disabledTouchUp = true;
            }
            break;

        case TOUCH_HOLD:
            break;
    }
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