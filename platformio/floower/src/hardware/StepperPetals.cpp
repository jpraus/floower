#include "Petals.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "StepperPetals";
#endif

#define TMC_EN_PIN 33
#define TMC_STEP_PIN 18
#define TMC_DIR_PIN 19
#define TMC_UART_RX_PIN 26
#define TMC_UART_TX_PIN 14
#define TMC_DRIVER_ADDRESS 0b00 // TMC2209 Driver address according to MS1 and MS2
#define TMC_R_SENSE 0.13f       // Match to your driver Rsense
#define TMC_MICROSTEPS 32
#define TMC_OPEN_STEPS 30000

StepperPetals::StepperPetals(Config *config) : config(config), stepperDriver(&Serial1, TMC_R_SENSE, TMC_DRIVER_ADDRESS), stepperMotion(AccelStepper::DRIVER, TMC_STEP_PIN, TMC_DIR_PIN) {
    Serial1.begin(500000, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN);
}

void StepperPetals::init(long currentPosition) {
    petalsOpenLevel = 0; // 0-100%

    // stepper
    enabled = false; // to make setStepperPowerOn effective
    setEnabled(true);
    pinMode(TMC_EN_PIN, OUTPUT);

    // verify stepper is available
    if (stepperDriver.testConnection()) {
        ESP_LOGE(LOG_TAG, "TMC2300 comm failed");
    }

    REG_IOIN iont = stepperDriver.readIontReg();
    Serial.print("IOIN=");
    Serial.println(iont.sr, BIN);
    Serial.print("Version=");
    Serial.println(iont.version);

    REG_CHOPCONF chopconf = stepperDriver.readChopconf();
    Serial.print("Microsteps=");
    Serial.println(chopconf.getMicrosteps());
    chopconf.setMicrosteps(TMC_MICROSTEPS);
    stepperDriver.writeChopconfReg(chopconf);

    stepperMotion.setPinsInverted(true, false, false);
    stepperMotion.setMaxSpeed(4000);
    stepperMotion.setSpeed(4000);
    stepperMotion.setAcceleration(4000);
    stepperMotion.setCurrentPosition(currentPosition); // closed (-> positive is open)
    stepperMotion.moveTo(0);
}

void StepperPetals::update() {
    if (stepperMotion.distanceToGo() != 0) {
        //stepperMotion.runSpeed();
        stepperMotion.run();
    }
}

void StepperPetals::setPetalsOpenLevel(int8_t level, int transitionTime) {
    ESP_LOGI(LOG_TAG, "Petals %d%%->%d%%", petalsOpenLevel, level);

    if (level == petalsOpenLevel) {
        return; // no change, keep doing the old movement until done
    }
    petalsOpenLevel = level;

    if (level >= 100) {
        stepperMotion.moveTo(TMC_OPEN_STEPS);
    }
    else {
        // TODO: calculate speed according to transitionTime
        stepperMotion.moveTo(level * TMC_OPEN_STEPS / 100);
    }
}

int8_t StepperPetals::getPetalsOpenLevel() {
    return petalsOpenLevel;
}

int8_t StepperPetals::getCurrentPetalsOpenLevel() {
    if (stepperMotion.distanceToGo() != 0) {
        float position = stepperMotion.currentPosition();
        return (position / TMC_OPEN_STEPS) * 100;
    }
    // optimization, no need to calculate the actual position when movement is finished
    return petalsOpenLevel;
}

bool StepperPetals::arePetalsMoving() {
    return stepperMotion.distanceToGo() != 0;
}

bool StepperPetals::setEnabled(bool enabled) {
    if (enabled && !this->enabled) {
        this->enabled = true;
        ESP_LOGD(LOG_TAG, "Stepper power ON");
        digitalWrite(TMC_EN_PIN, HIGH);
        return true;
    }
    if (!enabled && this->enabled) {
        this->enabled = false;
        ESP_LOGD(LOG_TAG, "Stepper power OFF");
        digitalWrite(TMC_EN_PIN, LOW);
        return true;
    }
    return false; // no change
}