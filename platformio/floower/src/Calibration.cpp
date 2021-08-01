#include "Calibration.h"

#define CALIBRATION_STATE_SERIAL 0
#define CALIBRATION_STATE_TOUCH 1

Calibration::Calibration(Floower *floower, Config *config) : floower(floower), config(config) {
    state = CALIBRATION_STATE_TOUCH;
}

void Calibration::calibrate() {
    switch (state) {
        case CALIBRATION_STATE_SERIAL:
            calibrateOverSerial();
            touch.samples = 0;
            touch.nextSampleMillis = 0;
            touch.value = 0;
            break;

        case CALIBRATION_STATE_TOUCH:
            calibrateTouch();
            break;
    }
}

void Calibration::calibrateTouch() {
    unsigned long now = millis();
    
    if (touch.samples == 0) {
        floower->readTouch(); // warm up
        touch.value = 0;
        touch.nextSampleMillis = now + 500;
        touch.samples++;
        floower->flashColor(colorGreen.H, colorGreen.S, 1000);
        ESP_LOGI(LOG_TAG, "Going to calibrate touch");
    }
    else if (touch.samples < 11) { // read ready value (10 samples)
        if (touch.nextSampleMillis < now) {
            uint8_t value = floower->readTouch();
            touch.value += value;
            touch.nextSampleMillis = now + 500;
            touch.samples++;
            ESP_LOGI(LOG_TAG, "Steady touch: value=%d", value);
        }
    }
    else if (touch.samples == 11) { // calibrate touch
        uint8_t touchThreshold = touch.value / 10;
        config->setTouchThreshold(touchThreshold - 5);
        config->commit(); // TODO
        ESP_LOGI(LOG_TAG, "Steady touch: value=%d, threshold=%d", touch.value, config->touchThreshold);
        state = CALIBRATION_STATE_SERIAL;
        floower->flashColor(colorPurple.H, colorPurple.S, 1000);
    }
}

void Calibration::calibrateOverSerial() {
    if (Serial.available() > 0) {
        char data = Serial.read();
        if (serialCommand == 0) { // start of command
            serialCommand = data;
            serialValue = "";
        }
        else if (data == '\n'){ // end of command
            long value = serialValue.toInt();
            if (serialCommand == 'T') { // touch threshold auto-detect
                state = CALIBRATION_STATE_TOUCH;
                touch.samples = 0;
                touch.nextSampleMillis = 0;
                touch.value = 0;
            }
            if (serialCommand == 'N') { // serial number
                ESP_LOGI(LOG_TAG, "New S/N %d", value);
                config->serialNumber = value;
            }
            else if (serialCommand == 'H') { // hardware revision
                ESP_LOGI(LOG_TAG, "New HW revision %d", value);
                config->hardwareRevision = value;
            }
            else if (serialCommand == 'E') { // end of calibration
                //config->hardwareCalibration(config->servoClosed, config->servoOpen, config->hardwareRevision, config->serialNumber);
                config->hardwareCalibration(config->hardwareRevision, config->serialNumber);
                config->factorySettings();
                config->setCalibrated();
                config->commit();
                ESP_LOGI(LOG_TAG, "Calibration done");
                ESP.restart(); // restart now
            }
            serialCommand = 0;
        }
        else { // command value
            serialValue += data;
        }
    }
}