#include "behavior/TestBehavior.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "TestBehavior";
#endif

#define STATE_BLOOM 128
#define STATE_FADE 129

TestBehavior::TestBehavior(Config *config, Floower *floower, BluetoothControl *bluetoothControl) 
        : SmartPowerBehavior(config, floower, bluetoothControl) {
}

void TestBehavior::loop() {
    SmartPowerBehavior::loop();

    unsigned long now = millis();
    if (eventTime <= now) {
        if (state == STATE_BLOOM) {
            floower->setPetalsOpenLevel(0, 5000);
            eventTime = now + 5000;
            changeState(STATE_FADE);
        }
        else if (state == STATE_FADE) {
            HsbColor nextColor = nextRandomColor();
            floower->transitionColor(nextColor.H, nextColor.S, config->colorBrightness, 5000);
            floower->setPetalsOpenLevel(100, 5000);
            eventTime = now + 5000;
            changeState(STATE_BLOOM);
        }
    }
}

bool TestBehavior::onLeafTouch(FloowerTouchEvent event) {
    if (SmartPowerBehavior::onLeafTouch(event)) {
        return true;
    }
    else if (event == TOUCH_DOWN && state == STATE_STANDBY) {
        eventTime = millis();
        changeState(STATE_FADE);
        return true;
    }
    return false;
}

bool TestBehavior::canInitializeBluetooth() {
    return false;
}