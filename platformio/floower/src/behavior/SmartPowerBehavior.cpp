#include "behavior/SmartPowerBehavior.h"
#include <esp_task_wdt.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "SmartPowerBehavior";
#endif

#define INDICATE_STATUS_IDLE 0
#define INDICATE_STATUS_CHARGING 1
#define INDICATE_STATUS_BLUETOOTH 2
#define INDICATE_STATUS_WIFI 3

// POWER MANAGEMENT (tuned for 1600mAh LIPO battery)

#define LOW_BATTERY_THRESHOLD_V 3.4

// TIMINGS

#define BLUETOOTH_START_DELAY 2000 // delay init of BLE to lower the power surge on startup
#define WIFI_START_DELAY 2500 // delay init of WiFi to lower the power surge on startup
#define DEEP_SLEEP_INACTIVITY_TIMEOUT 60000 // fall in deep sleep after timeout
#define LOW_BATTERY_WARNING_DURATION 5000 // how long to show battery dead status
#define WATCHDOGS_INTERVAL 1000

SmartPowerBehavior::SmartPowerBehavior(Config *config, Floower *floower, RemoteControl *remoteControl)
        : config(config), floower(floower), remoteControl(remoteControl) {
    state = STATE_OFF;
}

void SmartPowerBehavior::setup(bool wokeUp) {
    // check if there is enough power to run
    powerState = floower->readPowerState();
    if (!powerState.usbPowered && powerState.batteryVoltage < LOW_BATTERY_THRESHOLD_V) {
        delay(500); // wait and re-verify the voltage to make sure the battery is really dead
    }

    // run power watchdog to initialize state according to power
    powerWatchDog(true, wokeUp);

    if (state == STATE_STANDBY) {
        // normal operation
        ESP_LOGI(LOG_TAG, "Ready");
    }

    // run watchdog at periodic intervals
    watchDogsTime = millis() + WATCHDOGS_INTERVAL; // TODO millis overflow
}

void SmartPowerBehavior::loop() {
    // reset remote control state in case the floower is idle
    if (state == STATE_REMOTE_CONTROL && !floower->isLit() && !floower->isAnimating() && floower->getCurrentPetalsOpenLevel() == 0) {
        changeState(STATE_STANDBY);
    }

    // timers
    long now = millis();
    if (watchDogsTime < now) {
        watchDogsTime = now + WATCHDOGS_INTERVAL;
        esp_task_wdt_reset(); // reset watchdog timer
        powerWatchDog();
        indicateStatus(INDICATE_STATUS_BLUETOOTH, remoteControl->isBluetoothConnected());
        indicateStatus(INDICATE_STATUS_WIFI, remoteControl->isWifiConnected());
        //indicateStatus(INDICATE_STATUS_ACTY, true);
    }
    if (bluetoothStartTime > 0 && bluetoothStartTime < now && !floower->arePetalsMoving()) {
        bluetoothStartTime = 0;
        remoteControl->enableBluetooth();
    }
    if (wifiStartTime > 0 && wifiStartTime < now && !floower->arePetalsMoving()) {
        wifiStartTime = 0;
        remoteControl->enableWifi();
    }
    if (deepSleepTime != 0 && deepSleepTime < now) {
        deepSleepTime = 0;
        if (!powerState.usbPowered) {
            enterDeepSleep();
        }
    }
}

bool SmartPowerBehavior::onLeafTouch(FloowerTouchEvent event) {
    if (event == FloowerTouchEvent::TOUCH_HOLD && config->bluetoothEnabled && canInitializeBluetooth()) {
        floower->flashColor(colorBlue.H, colorBlue.S, 1000);
        remoteControl->enableBluetooth();
        changeState(STATE_BLUETOOTH_PAIRING);
        return true;
    }
    else if (event == FloowerTouchEvent::TOUCH_DOWN && state == STATE_BLUETOOTH_PAIRING) {
        // bluetooth pairing interrupted
        remoteControl->disableBluetooth();
        config->setBluetoothAlwaysOn(false);
        floower->transitionColorBrightness(0, 500);
        changeState(STATE_STANDBY);
        preventTouchUp = true;
        return true;
    }
    else if (preventTouchUp && event == TOUCH_UP) {
        preventTouchUp = false;
        return true;
    };

    return false;
}

void SmartPowerBehavior::onRemoteControl() {
    changeState(STATE_REMOTE_CONTROL);
}

bool SmartPowerBehavior::canInitializeBluetooth() {
    return state == STATE_STANDBY;
}

void SmartPowerBehavior::enablePeripherals(bool initial, bool wokeUp) {
    floower->initPetals(initial, wokeUp); // TODO
    floower->enableTouch([=](FloowerTouchEvent event){ onLeafTouch(event); }, !wokeUp);
    remoteControl->onRemoteControl([=]() { onRemoteControl(); });
    if (config->bluetoothEnabled && config->bluetoothAlwaysOn) {
        bluetoothStartTime = millis() + BLUETOOTH_START_DELAY; // defer init of BLE by 5 seconds
    }
}

void SmartPowerBehavior::disablePeripherals() {
    floower->disableTouch();
    remoteControl->disableBluetooth();
    remoteControl->disableWifi();
    // TODO: disconnect remote
    // TODO: disable petals?
}

bool SmartPowerBehavior::isIdle() {
    return !floower->arePetalsMoving() && !floower->isChangingColor();
}

void SmartPowerBehavior::powerWatchDog(bool initial, bool wokeUp) {
    powerState = floower->readPowerState();

    if (!powerState.usbPowered && powerState.batteryVoltage < LOW_BATTERY_THRESHOLD_V) {
        // not powered by USB (switch must be ON) and low battery (* -> OFF)
        if (state != STATE_LOW_BATTERY) {
            ESP_LOGW(LOG_TAG, "Shutting down, battery low voltage (%dV)", powerState.batteryVoltage);
            floower->flashColor(colorRed.H, colorRed.S, 1000);
            floower->setPetalsOpenLevel(0, 2500);
            disablePeripherals();
            changeState(STATE_LOW_BATTERY);
            planDeepSleep(LOW_BATTERY_WARNING_DURATION);
        }
    }
    else if (!powerState.switchedOn) {
        // power by USB but switch is OFF (* -> OFF)
        if (state != STATE_OFF) {
            ESP_LOGW(LOG_TAG, "Switched OFF");
            floower->transitionColorBrightness(0, 2500);
            floower->setPetalsOpenLevel(0, 2500);
            disablePeripherals();
            changeState(STATE_OFF);
        }
    }
    else {
        // powered by USB or battery and switch is ON
        if (state == STATE_OFF || (state == STATE_LOW_BATTERY && powerState.usbPowered)) {
            // turned ON or connected to USB on low battery
            ESP_LOGI(LOG_TAG, "Power restored");
            floower->stopAnimation(false); // in case of low battery blinking
            enablePeripherals(initial, wokeUp);
            changeState(STATE_STANDBY);
        }
        else if (state == STATE_STANDBY && !powerState.usbPowered && deepSleepTime == 0) {
            // powered by battery and deep sleep is not yet planned
            planDeepSleep(DEEP_SLEEP_INACTIVITY_TIMEOUT);
        }
        if (config->wifiEnabled && powerState.usbPowered && !remoteControl->isWifiEnabled() && wifiStartTime == 0) {
            wifiStartTime = millis() + WIFI_START_DELAY;
        }
        if (!powerState.usbPowered) {
            remoteControl->disableWifi();
        }
    }

    remoteControl->updateStatusData(powerState.batteryLevel, powerState.batteryCharging);
    indicateStatus(INDICATE_STATUS_CHARGING, powerState.batteryCharging);
}

void SmartPowerBehavior::changeStateIfIdle(state_t fromState, state_t toState) {
    if (state == fromState && !floower->arePetalsMoving() && !floower->isChangingColor()) {
        changeState(toState);
    }
}

void SmartPowerBehavior::changeState(uint8_t newState) {
    if (state != newState) {
        state = newState;
        ESP_LOGI(LOG_TAG, "Changed state to %d", newState);

        if (!powerState.usbPowered && state == STATE_STANDBY) {
            planDeepSleep(DEEP_SLEEP_INACTIVITY_TIMEOUT);
        }
        else if (deepSleepTime > 0) {
            ESP_LOGI(LOG_TAG, "Sleep interrupted");
            deepSleepTime = 0;
        }
    }
}

void SmartPowerBehavior::indicateStatus(uint8_t status, bool enable) {
    if (enable && indicatingStatus != status) {
        switch (status) {
            case INDICATE_STATUS_CHARGING: // charging has the top priotity
                floower->showStatus(colorRed, FloowerStatusAnimation::PULSATING, 2000);
                indicatingStatus = status;
                break;
            case INDICATE_STATUS_BLUETOOTH:
                if (indicatingStatus != INDICATE_STATUS_CHARGING) {
                    floower->showStatus(colorBlue, FloowerStatusAnimation::PULSATING, 2000);
                    indicatingStatus = status;
                }
                break;
            case INDICATE_STATUS_WIFI:
                if (indicatingStatus != INDICATE_STATUS_CHARGING && indicatingStatus != INDICATE_STATUS_BLUETOOTH) {
                    floower->showStatus(colorPurple, FloowerStatusAnimation::PULSATING, 2000);
                    indicatingStatus = status;
                }
                break;
        }
    }
    else if (!enable && indicatingStatus == status) {
        indicatingStatus = INDICATE_STATUS_IDLE; // idle status
        floower->showStatus(colorBlack, FloowerStatusAnimation::STILL, 0);
    }
}

void SmartPowerBehavior::planDeepSleep(long timeoutMs) {
    if (config->deepSleepEnabled) {
        deepSleepTime = millis() + timeoutMs;
        ESP_LOGI(LOG_TAG, "Sleep in %d", timeoutMs);
    }
}

void SmartPowerBehavior::enterDeepSleep() {
    ESP_LOGI(LOG_TAG, "Going to sleep now");
    floower->beforeDeepSleep();
    esp_sleep_enable_touchpad_wakeup();
    //esp_wifi_stop();
    btStop();
    esp_deep_sleep_start();
}

HsbColor SmartPowerBehavior::nextRandomColor() {
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