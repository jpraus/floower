#include "StateMachine.h"
#include <esp_task_wdt.h>

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "StateMachine";
#endif

// STATES

#define STATE_STANDBY 0
#define STATE_RUNNING 1
#define STATE_LOW_BATTERY 2
#define STATE_SOFT_POWER_OFF 3
#define STATE_BLUETOOTH_PAIRING 4

#define INDICATE_STATUS_ACTY 0
#define INDICATE_STATUS_CHARGING 1
#define INDICATE_STATUS_REMOTE 2

// POWER MANAGEMENT

// tuned for 1600mAh LIPO battery
#define LOW_BATTERY_THRESHOLD_V 3.4

// TIMINGS

#define REMOTE_INIT_TIMEOUT 2000 // delay init of BLE to lower the power surge on startup
#define DEEP_SLEEP_INACTIVITY_TIMEOUT 60000 // fall in deep sleep after timeout
#define LOW_BATTERY_WARNING_DURATION 5000 // how long to show battery dead status
#define WATCHDOGS_INTERVAL 1000

StateMachine::StateMachine(Config *config, Floower *floower, Remote *remote)
        : config(config), floower(floower), remote(remote) {
    softPower = false;
    lowBattery = false;
    state = STATE_STANDBY;
}

void StateMachine::init(bool wokeUp) {
    // check if there is enough power to run
    PowerState powerState = floower->readPowerState();
    if (!powerState.usbPowered && powerState.batteryVoltage < LOW_BATTERY_THRESHOLD_V) {
        delay(500); // wait and re-verify the voltage to make sure the battery is really dead
    }

    // run power watchdog to initialize state according to power
    powerWatchDog(wokeUp);

    /*else if (!config.calibrated) {
        // hardware is not calibrated
        ESP_LOGW(LOG_TAG, "Floower not calibrated");
        calibration.setup();
        floower.flashColor(colorPurple.H, colorPurple.S, 2000);
        // TODO
        floower.initStepper(35000); // open steps, force the Floower to close
        ESP_LOGI(LOG_TAG, "Ready for calibration");
    }*/
    if (state == STATE_STANDBY) {
        // normal operation
        ESP_LOGI(LOG_TAG, "Ready");
    }

    // run watchdog at periodic intervals
    watchDogsTime = millis() + WATCHDOGS_INTERVAL; // TODO millis overflow
}

void StateMachine::update() {
    // timers
    long now = millis();
    if (watchDogsTime < now) {
        watchDogsTime = now + WATCHDOGS_INTERVAL;
        esp_task_wdt_reset(); // reset watchdog timer
        powerWatchDog();
        indicateStatus(INDICATE_STATUS_REMOTE, remote->isConnected());
        indicateStatus(INDICATE_STATUS_ACTY, true);
    }
    if (initRemoteTime != 0 && initRemoteTime < now && !floower->arePetalsMoving()) {
        initRemoteTime = 0;
        remote->init();
        remote->startAdvertising();
    }
    if (deepSleepTime != 0 && deepSleepTime < now) {
    // TODO: if (deepSleepTime != 0 && deepSleepTime < now && !floower->arePetalsMoving()) {
        deepSleepTime = 0;
        enterDeepSleep();
    }
}

void StateMachine::onLeafTouch(FloowerTouchEvent event) {
    // TODO
}

void StateMachine::enablePeripherals(bool wokeUp) {
    floower->initStepper();
    floower->enableTouch([=](FloowerTouchEvent event){ onLeafTouch(event); }, !wokeUp);
    //remote->onTakeOver([=]() { onRemoteTookOver(); }); // remote controller took over
    if (config->bluetoothEnabled && config->initRemoteOnStartup) {
        initRemoteTime = millis() + REMOTE_INIT_TIMEOUT; // defer init of BLE by 5 seconds
    }
}

void StateMachine::disablePeripherals() {
    uint16_t speed = config->speedMillis / 2;
    floower->transitionColorBrightness(0, speed);
    floower->setPetalsOpenLevel(0, speed);
    floower->disableTouch();
    // TODO: disconnect remote
    // TODO: disable petals?
}

bool StateMachine::isIdle() {
    return false;
}

void StateMachine::powerWatchDog(bool wokeUp) {
    PowerState powerState = floower->readPowerState();
    remote->setBatteryLevel(powerState.batteryLevel, powerState.batteryCharging);
    indicateStatus(INDICATE_STATUS_CHARGING, powerState.batteryCharging);

    if (!powerState.usbPowered && powerState.batteryVoltage < LOW_BATTERY_THRESHOLD_V) {
        // low battery (* -> LOW_BATTERY)
        if (state != STATE_LOW_BATTERY) {
            // shutdown
            ESP_LOGW(LOG_TAG, "Shutting down, battery low voltage (%dV)", powerState.batteryVoltage);
            if (powerState.switchedOn) {
                floower->flashColor(colorRed.H, colorRed.S, 1000);
                floower->setPetalsOpenLevel(0, 2500);
            }
            changeState(STATE_LOW_BATTERY);
            planDeepSleep(LOW_BATTERY_WARNING_DURATION);
        }
    }
    else if (!powerState.switchedOn) {
        // power by USB but switched off (STANDBY, BLUETOOTH_PAIRING, RUNNING* -> SOFT_POWER_OFF)
        if (state != STATE_SOFT_POWER_OFF) {
            ESP_LOGW(LOG_TAG, "Switched OFF");
            disablePeripherals();
            changeState(STATE_SOFT_POWER_OFF);
        }
    }
    else if (powerState.usbPowered) {
        // powered by USB // LOW_BATTERY, SOFT_POWER_OFF -> STANDBY
        if (state == STATE_LOW_BATTERY) {
            ESP_LOGI(LOG_TAG, "Power restored");
            enablePeripherals(wokeUp);
            changeState(STATE_STANDBY);
        }
        else if (state == STATE_SOFT_POWER_OFF) {
            ESP_LOGI(LOG_TAG, "Switched ON");
            enablePeripherals(wokeUp);
            changeState(STATE_STANDBY);
        }
        unplanDeepSleep(); // should not enter deep sleep while being powered by USB
    }
    else {
        // powered by battery
        if (state == STATE_STANDBY && deepSleepTime == 0) {
            planDeepSleep(DEEP_SLEEP_INACTIVITY_TIMEOUT);
        }
    }
}

void StateMachine::changeState(uint8_t newState) {
    if (state != newState) {
        state = newState;
        ESP_LOGD(LOG_TAG, "Changed state to %d", newState);
    }
    if (state != STATE_STANDBY && state != STATE_LOW_BATTERY) {
        unplanDeepSleep(); // can enter deep sleep only from standby and low battery state
    }
}

void StateMachine::indicateStatus(uint8_t status, bool enable) {
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

void StateMachine::planDeepSleep(long timeoutMs) {
    deepSleepTime = millis() + timeoutMs;
    ESP_LOGI(LOG_TAG, "Sleep in %d", timeoutMs);
}

void StateMachine::unplanDeepSleep() {
    if (deepSleepTime > 0) {
        ESP_LOGI(LOG_TAG, "Sleep interrupted");
        deepSleepTime = 0;
    }
}

void StateMachine::enterDeepSleep() {
    ESP_LOGI(LOG_TAG, "Going to sleep now");
    esp_sleep_enable_touchpad_wakeup();
    //esp_wifi_stop();
    btStop();
    esp_deep_sleep_start();
}