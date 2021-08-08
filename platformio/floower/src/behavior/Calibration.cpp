#include "behavior/Calibration.h"
#include <esp_task_wdt.h>

#define STATE_LISTENING 0
#define STATE_CALIBRATE_TOUCH_FIRST_SAMPLE 1
#define STATE_CALIBRATE_TOUCH_LAST_SAMPLE 12

// TIMINGS

#define TOUCH_SAMPLING_INTERVAL 500 // 0.5S
#define WATCHDOGS_INTERVAL 1000

Calibration::Calibration(Config *config, Floower *floower)
        : config(config), floower(floower) {
}

void Calibration::init(bool wokeUp) {
    state = STATE_LISTENING;
    floower->flashColor(colorPurple.H, colorPurple.S, 1000);
}

void Calibration::update() {
    if (state == STATE_LISTENING) {
        calibrateListenSerial();
    }
    else if (state >= STATE_CALIBRATE_TOUCH_FIRST_SAMPLE && state <= STATE_CALIBRATE_TOUCH_LAST_SAMPLE) {
        calibrateTouch();
    }

    // timers
    long now = millis();
    if (watchDogsTime < now) {
        watchDogsTime = now + WATCHDOGS_INTERVAL;
        esp_task_wdt_reset(); // reset watchdog timer
    }
}

bool Calibration::isIdle() {
    return false;
}

void Calibration::calibrateListenSerial() {
    if (Serial.available() > 0) {
        char data = Serial.read();
        if (command == 0) { // start of command
            command = data;
            serialValue = "";
        }
        else if (data == '\n'){ // end of command
            long value = serialValue.toInt();
            if (command == 'T') { // touch threshold auto-detect
                ESP_LOGI(LOG_TAG, "Going to calibrate touch sensor");
                state = STATE_CALIBRATE_TOUCH_FIRST_SAMPLE;
            }
            else if (command == 'C') { // servo closed angle limit
                if (value > 0) {
                    ESP_LOGI(LOG_TAG, "New closed angle %d", value);
                    //floower->setPetalsAngle(value, abs(value - floower->getCurrentPetalsAngle()) * 4);
                    //config->servoClosed = value;
                }
            }
            else if (command == 'O') { // servo open angle limit
                if (value > 0) {
                    ESP_LOGI(LOG_TAG, "New open angle %d", value);
                    //floower->setPetalsAngle(value, abs(value - floower->getCurrentPetalsAngle()) * 4);
                    //config->servoOpen = value;
                }
            }
            else if (command == 'N') { // serial number
                ESP_LOGI(LOG_TAG, "New S/N %d", value);
                config->serialNumber = value;
            }
            else if (command == 'H') { // hardware revision
                ESP_LOGI(LOG_TAG, "New HW revision %d", value);
                config->hardwareRevision = value;
            }
            else if (command == 'E') { // end of calibration
                config->hardwareCalibration(config->servoClosed, config->servoOpen, config->hardwareRevision, config->serialNumber);
                config->factorySettings();
                config->setCalibrated();
                config->commit();
                ESP_LOGI(LOG_TAG, "Calibration done");
                ESP.restart(); // restart now
            }
            command = 0;
        }
        else { // command value
            serialValue += data;
        }
    }
}

void Calibration::calibrateTouch() {
    unsigned long now = millis();

    if (state == STATE_CALIBRATE_TOUCH_FIRST_SAMPLE) {
        floower->readTouch(); // warm up
        touchValue = 0;
        touchSampleMillis = now + TOUCH_SAMPLING_INTERVAL;
        state++;
        floower->flashColor(colorGreen.H, colorGreen.S, 1000);
    }
    else if (state < STATE_CALIBRATE_TOUCH_LAST_SAMPLE) { // read not-touched value (10 samples)
        if (touchSampleMillis < now) {
            uint8_t value = floower->readTouch();
            touchValue += value;
            touchSampleMillis = now + TOUCH_SAMPLING_INTERVAL;
            state++;
            ESP_LOGI(LOG_TAG, "Touch sensor: value=%d", value);
        }
    }
    else { // calibratin done, write value
        uint8_t touchThreshold = touchValue / 10;
        config->setTouchThreshold(touchThreshold - 5);
        ESP_LOGI(LOG_TAG, "Touch calibration: value=%d, threshold=%d", touchValue, config->touchThreshold);
        state = STATE_LISTENING;
        floower->flashColor(colorPurple.H, colorPurple.S, 1000);
    }
}