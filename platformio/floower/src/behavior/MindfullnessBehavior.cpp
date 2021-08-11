#include "behavior/MindfullnessBehavior.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "MindfullnessBehavior";
#endif

#define STATE_INHALE 128
#define STATE_EXHALE 129

MindfullnessBehavior::MindfullnessBehavior(Config *config, Floower *floower, BluetoothControl *bluetoothControl) 
        : SmartPowerBehavior(config, floower, bluetoothControl) {
}

void MindfullnessBehavior::setup(bool wokeUp) {
    SmartPowerBehavior::setup(wokeUp);
    eventTime = millis();
}

void MindfullnessBehavior::loop() {
    SmartPowerBehavior::loop();

    unsigned long now = millis();
    if (eventTime <= now) {
        if (state == STATE_INHALE || state == STATE_STANDBY) {
            floower->transitionColor(colorPurple.H, colorPurple.S, config->colorBrightness, 5000);
            floower->setPetalsOpenLevel(100, 5000);
            eventTime = now + 5000;
            changeState(STATE_EXHALE);
        }
        else if (state == STATE_EXHALE) {
            floower->transitionColorBrightness(0, 5000);
            floower->setPetalsOpenLevel(0, 5000);
            eventTime = now + 5000;
            changeState(STATE_INHALE);
        }
    }
}

bool MindfullnessBehavior::onLeafTouch(FloowerTouchEvent event) {
    if (SmartPowerBehavior::onLeafTouch(event)) {
        return true;
    }
    else {
        // TODO
    }
    return false;
}