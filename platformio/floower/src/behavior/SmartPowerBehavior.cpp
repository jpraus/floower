#include "behavior/SmartPowerBehavior.h"
#include <esp_task_wdt.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "SmartPowerBehavior";
#endif

#define INDICATE_STATUS_ACTY 0
#define INDICATE_STATUS_CHARGING 1
#define INDICATE_STATUS_REMOTE 2

// POWER MANAGEMENT (tuned for 1600mAh LIPO battery)

#define LOW_BATTERY_THRESHOLD_V 3.4

// TIMINGS

#define REMOTE_INIT_TIMEOUT 2000 // delay init of BLE to lower the power surge on startup
#define DEEP_SLEEP_INACTIVITY_TIMEOUT 60000 // fall in deep sleep after timeout
#define LOW_BATTERY_WARNING_DURATION 5000 // how long to show battery dead status
#define WATCHDOGS_INTERVAL 1000

SmartPowerBehavior::SmartPowerBehavior(Config *config, Floower *floower, BluetoothControl *bluetoothControl)
        : config(config), floower(floower), bluetoothControl(bluetoothControl) {
    state = STATE_OFF;
}

void SmartPowerBehavior::setup(bool wokeUp) {
    // check if there is enough power to run
    powerState = floower->readPowerState();
    if (!powerState.usbPowered && powerState.batteryVoltage < LOW_BATTERY_THRESHOLD_V) {
        delay(500); // wait and re-verify the voltage to make sure the battery is really dead
    }

    // run power watchdog to initialize state according to power
    powerWatchDog(wokeUp);

    if (state == STATE_STANDBY) {
        // normal operation
        ESP_LOGI(LOG_TAG, "Ready");
    }

    // run watchdog at periodic intervals
    watchDogsTime = millis() + WATCHDOGS_INTERVAL; // TODO millis overflow
}

void SmartPowerBehavior::loop() {
    // timers
    long now = millis();
    if (watchDogsTime < now) {
        watchDogsTime = now + WATCHDOGS_INTERVAL;
        esp_task_wdt_reset(); // reset watchdog timer
        powerWatchDog();
        indicateStatus(INDICATE_STATUS_REMOTE, bluetoothControl->isConnected());
        indicateStatus(INDICATE_STATUS_ACTY, true);
    }
    if (initRemoteTime != 0 && initRemoteTime < now && !floower->arePetalsMoving()) {
        initRemoteTime = 0;
        bluetoothControl->init();
        bluetoothControl->startAdvertising();
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
        bluetoothControl->init();
        bluetoothControl->startAdvertising();
        changeState(STATE_BLUETOOTH_PAIRING);
        return true;
    }
    else if (event == FloowerTouchEvent::TOUCH_DOWN && state == STATE_BLUETOOTH_PAIRING) {
        // bluetooth pairing interrupted
        bluetoothControl->stopAdvertising();
        config->setRemoteOnStartup(false);
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

bool SmartPowerBehavior::canInitializeBluetooth() {
    return state == STATE_STANDBY;
}

void SmartPowerBehavior::enablePeripherals(bool wokeUp) {
    floower->initPetals();
    floower->enableTouch([=](FloowerTouchEvent event){ onLeafTouch(event); }, !wokeUp);
    //bluetoothControl->onTakeOver([=]() { onRemoteTookOver(); }); // bluetoothControl controller took over
    if (config->bluetoothEnabled && config->initRemoteOnStartup) {
        initRemoteTime = millis() + REMOTE_INIT_TIMEOUT; // defer init of BLE by 5 seconds
    }
}

void SmartPowerBehavior::disablePeripherals() {
    floower->disableTouch();
    bluetoothControl->stopAdvertising();
    // TODO: disconnect remote
    // TODO: disable petals?
}

bool SmartPowerBehavior::isIdle() {
    return state == STATE_STANDBY;
}

void SmartPowerBehavior::powerWatchDog(bool wokeUp) {
    powerState = floower->readPowerState();

    if (!powerState.usbPowered && powerState.batteryVoltage < LOW_BATTERY_THRESHOLD_V) {
        // not powered by USB (switch must be ON) and low battery (* -> OFF)
        if (state != STATE_OFF) {
            ESP_LOGW(LOG_TAG, "Shutting down, battery low voltage (%dV)", powerState.batteryVoltage);
            floower->flashColor(colorRed.H, colorRed.S, 1000);
            floower->setPetalsOpenLevel(0, 2500);
            disablePeripherals();
            changeState(STATE_OFF);
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
        if (state == STATE_OFF) {
            ESP_LOGI(LOG_TAG, "Power restored");
            enablePeripherals(wokeUp);
            changeState(STATE_STANDBY);
        }
        else if (state == STATE_STANDBY && !powerState.usbPowered && deepSleepTime == 0) {
            planDeepSleep(DEEP_SLEEP_INACTIVITY_TIMEOUT);
        }
    }

    bluetoothControl->setBatteryLevel(powerState.batteryLevel, powerState.batteryCharging);
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
        ESP_LOGD(LOG_TAG, "Changed state to %d", newState);

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
    if (enable) {
        if (status == INDICATE_STATUS_ACTY && indicatingStatus == INDICATE_STATUS_ACTY) {
            floower->showStatus(colorPurple, FloowerStatusAnimation::BLINK_ONCE, 50);
        }
        else if (indicatingStatus != status) {
            switch (status) {
                case INDICATE_STATUS_CHARGING: // charging has the top priotity
                    floower->showStatus(colorRed, FloowerStatusAnimation::PULSATING, 2000);
                    indicatingStatus = status;
                    break;
                case INDICATE_STATUS_REMOTE:
                    if (indicatingStatus != INDICATE_STATUS_CHARGING) {
                        floower->showStatus(colorBlue, FloowerStatusAnimation::PULSATING, 2000);
                        indicatingStatus = status;
                    }
                    break;
            }
        }
    }
    else if (indicatingStatus == status) {
        indicatingStatus = INDICATE_STATUS_ACTY; // idle status
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
    esp_sleep_enable_touchpad_wakeup();
    //esp_wifi_stop();
    btStop();
    esp_deep_sleep_start();
}