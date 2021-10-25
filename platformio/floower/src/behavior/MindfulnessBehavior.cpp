#include "behavior/MindfulnessBehavior.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "MindfulnessBehavior";
#endif

#define STATE_INHALE 128
#define STATE_EXHALE 129

MindfulnessBehavior::MindfulnessBehavior(Config *config, Floower *floower, BluetoothConnect *bluetoothConnect) 
        : SmartPowerBehavior(config, floower, bluetoothConnect) {
}

void MindfulnessBehavior::loop() {
    SmartPowerBehavior::loop();

    unsigned long now = millis();
    if (eventTime <= now) {
        if (state == STATE_INHALE) {
            floower->transitionColorBrightness(1, 3000);
            floower->setPetalsOpenLevel(70, 2800);
            eventTime = now + 3000;
            changeState(STATE_EXHALE);
        }
        else if (state == STATE_EXHALE) {
            floower->transitionColorBrightness(0.2, 5000);
            floower->setPetalsOpenLevel(20, 4800);
            eventTime = now + 5000;
            changeState(STATE_INHALE);
        }
    }
}

bool MindfulnessBehavior::onLeafTouch(FloowerTouchEvent event) {
    if (SmartPowerBehavior::onLeafTouch(event)) {
        return true;
    }
    else if (event == TOUCH_DOWN) {
        if (state == STATE_STANDBY) {
            floower->transitionColor(colorYellow.H, colorYellow.S, 0.1, 2000);
            floower->setPetalsOpenLevel(20, 2000);
            eventTime = millis() + 5000;
            changeState(STATE_INHALE);
        }
        else {
            floower->transitionColorBrightness(0, 5000);
            floower->setPetalsOpenLevel(0, 5000);
            changeState(STATE_STANDBY);
        }
    }
    return false;
}