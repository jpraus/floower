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

#define TMC_MIN_PULSE_WIDTH 1
#define DIRECTION_CW 1
#define DIRECTION_CCW -1

//#define STALLGUARD_SAMPLING_PERIOD 50

StepperPetals::StepperPetals(Config *config) : config(config), stepperDriver(&Serial1, TMC_R_SENSE, TMC_DRIVER_ADDRESS) {
    Serial1.begin(500000, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN);
    initialized = false;
    petalsOpenLevel = 0; // 0-100%
}

void StepperPetals::init(bool initial, bool wokeUp) {
    // stepper
    enabled = false; // to make setStepperPowerOn effective
    setEnabled(true); // it will be auto-disabled in update method
    pinMode(TMC_EN_PIN, OUTPUT);

    currentSteps = 0;
    targetSteps = 0;
    stepInterval = 200; // default speed
    lastStepTime = 0;
    pinMode(TMC_STEP_PIN, OUTPUT);
    digitalWrite(TMC_STEP_PIN, LOW);

    if (initial && !wokeUp) {
        // make sure the Floower is closed for the first time it's turned on
        currentSteps = 0; // TODO
    }

    direction = DIRECTION_CCW; // default is closing
    pinMode(TMC_DIR_PIN, OUTPUT);
    digitalWrite(TMC_DIR_PIN, HIGH);

    // verify stepper is available
    if (stepperDriver.testConnection()) {
        ESP_LOGE(LOG_TAG, "TMC2300 failed");
    }

    if (!initialized) {
        REG_IOIN iont = stepperDriver.readIontReg();
        ESP_LOGI(LOG_TAG, "TMC2300: v=%d", iont.version);

        REG_CHOPCONF chopconf = stepperDriver.readChopconf();
        chopconf.setMicrosteps(TMC_MICROSTEPS);
        chopconf.diss2vs = true; // HOTFIX
        chopconf.diss2g = true; // HOTFIX
        stepperDriver.writeChopconfReg(chopconf);

        REG_IHOLD_IRUN iholdIrun;
        iholdIrun.irun = 31;
        iholdIrun.ihold = 1;
        iholdIrun.iholddelay = 1;
        stepperDriver.writeIholdIrunReg(iholdIrun);

        initialized = true;
    }
}

void StepperPetals::update() {
    if (currentSteps != targetSteps) {
        runStepper();
        //detectStall();
    }
    else if (enabled) {
        setEnabled(false);
        sgTimer = 0;
    }
}

void StepperPetals::setPetalsOpenLevel(int8_t level, int transitionTime) {
    ESP_LOGI(LOG_TAG, "Petals %d%%->%d%%", petalsOpenLevel, level);
/*
    REG_GSTAT gstat = stepperDriver.readGStat();
    Serial.print("GSTAT=");
    Serial.println(gstat.sr, BIN);
    Serial.print("reset=");
    Serial.println(gstat.reset);
    Serial.print("drv_err=");
    Serial.println(gstat.drv_err);

    REG_DRV_STATUS drvStatus = stepperDriver.readDrvStatusReg();
    Serial.print("DRV_STATUS=");
    Serial.println(drvStatus.sr, BIN);
    Serial.print("s2vsa=");
    Serial.println(drvStatus.s2vsa);
    Serial.print("s2vsb=");
    Serial.println(drvStatus.s2vsb);
    Serial.print("s2ga=");
    Serial.println(drvStatus.s2ga);
    Serial.print("s2gb=");
    Serial.println(drvStatus.s2gb);
    Serial.print("ot=");
    Serial.println(drvStatus.ot);
*/
    if (level == petalsOpenLevel) {
        return; // no change, keep doing the old movement until done
    }
    petalsOpenLevel = level;

    if (level >= 100) {
        targetSteps = TMC_OPEN_STEPS;
    }
    else if (level <= 0) {
        currentSteps += 1000; // TODO: make sure the petals will close completelly
        targetSteps = 0;
    }
    else {
        targetSteps = level * TMC_OPEN_STEPS / 100;
    }

    // calculate the speed
    float stepsToTake = abs(targetSteps - currentSteps);
    float stepsPerMs = stepsToTake / transitionTime;
    stepInterval = fabs(1000.0 / stepsPerMs);

    // set direction upfront
    direction = targetSteps >= currentSteps ? DIRECTION_CW : DIRECTION_CCW;
    digitalWrite(TMC_DIR_PIN, direction == DIRECTION_CW ? LOW : HIGH);
    
    // enable
    setEnabled(true);
#ifdef STALLGUARD_SAMPLING_PERIOD
    sgTimer = millis() + STALLGUARD_SAMPLING_PERIOD;
#endif
}

int8_t StepperPetals::getPetalsOpenLevel() {
    return petalsOpenLevel;
}

int8_t StepperPetals::getCurrentPetalsOpenLevel() {
    if (currentSteps != targetSteps) {
        return (currentSteps / TMC_OPEN_STEPS) * 100;
    }
    // optimization, no need to calculate the actual position when movement is finished
    return petalsOpenLevel;
}

bool StepperPetals::arePetalsMoving() {
    return currentSteps != targetSteps;
}

bool StepperPetals::setEnabled(bool enabled) {
    if (enabled && !this->enabled) {
        this->enabled = true;
        ESP_LOGI(LOG_TAG, "Stepper enabled");
        digitalWrite(TMC_EN_PIN, HIGH);
        return true;
    }
    if (!enabled && this->enabled) {
        this->enabled = false;
        ESP_LOGI(LOG_TAG, "Stepper disabled");
        digitalWrite(TMC_EN_PIN, LOW);
        return true;
    }
    return false; // no change
}

bool StepperPetals::runStepper() {
    if (stepInterval <= 0) {
	    return false;
    }

    unsigned long time = micros();
    if (time - lastStepTime >= stepInterval) {
        currentSteps += direction;
        //Serial.println(currentSteps);

        // step
        digitalWrite(TMC_STEP_PIN, HIGH);
        // Caution 200ns setup time 
        // Delay the minimum allowed pulse width
        delayMicroseconds(TMC_MIN_PULSE_WIDTH);
        digitalWrite(TMC_STEP_PIN, LOW);

	    lastStepTime = time; // Caution: does not account for costs in step()
	    return true;
    }
	return false;
}

void StepperPetals::detectStall() {
#ifdef STALLGUARD_SAMPLING_PERIOD
    if (sgTimer > 0 && sgTimer < millis()) {
        uint8_t sgValue = stepperDriver.readSGValue();
        Serial.print("sgValue=");
        Serial.println(sgValue);
        sgTimer = millis() + STALLGUARD_SAMPLING_PERIOD;
    }
#endif
    // also check error conditions
}