#include <Arduino.h>
#include <tmc2300.h>
#include <NeoPixelBus.h>

const HsbColor colorRed(0.0, 1.0, 0.2);
const HsbColor colorGreen(0.3, 1.0, 0.2);
const HsbColor colorBlue(0.61, 1.0, 0.2);
const HsbColor colorYellow(0.16, 1.0, 0.2);
const HsbColor colorOrange(0.06, 1.0, 0.2);
const HsbColor colorWhite(0.0, 0.0, 0.2);
const HsbColor colorPurple(0.81, 1.0, 0.2);
const HsbColor colorPink(0.93, 1.0, 0.2);
const HsbColor colorBlack(0.0, 1.0, 0.0);

#define STATUS_NEOPIXEL_PIN GPIO_NUM_32

#define TMC_EN_PIN 33
#define TMC_STEP_PIN 18
#define TMC_DIR_PIN 19
#define TMC_UART_RX_PIN 26
#define TMC_UART_TX_PIN 14
#define TMC_DRIVER_ADDRESS 0b00 // TMC2209 Driver address according to MS1 and MS2
#define TMC_R_SENSE 0.13f       // Match to your driver Rsense
#define TMC_MICROSTEPS 32
#define TMC_OPEN_STEPS 60000

#define TMC_MIN_PULSE_WIDTH 1
#define DIRECTION_CW 1
#define DIRECTION_CCW -1

TMC2300 stepperDriver(&Serial1, TMC_R_SENSE, TMC_DRIVER_ADDRESS);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1800KbpsMethod> statusPixel(2, STATUS_NEOPIXEL_PIN);

// stepper state
int8_t direction; // 1 CW, -1 CCW
long targetSteps;
long currentSteps;
unsigned long lastStepTime;
unsigned long stepInterval;

void setup() {
    Serial.begin(115200);
    ESP_LOGI(LOG_TAG, "Initializing");
    delay(1000);

    pinMode(TMC_EN_PIN, OUTPUT);
    digitalWrite(TMC_EN_PIN, HIGH);

    currentSteps = 0;
    targetSteps = 0;
    stepInterval = 100; // default speed
    lastStepTime = 0;
    direction = DIRECTION_CCW; // default is closing
    pinMode(TMC_DIR_PIN, OUTPUT);
    digitalWrite(TMC_DIR_PIN, HIGH);
    pinMode(TMC_STEP_PIN, OUTPUT);
    digitalWrite(TMC_STEP_PIN, LOW);

    Serial1.begin(500000, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN);
    if (stepperDriver.testConnection()) {
        ESP_LOGE(LOG_TAG, "TMC2300 failed");
    }

    REG_IOIN iont = stepperDriver.readIontReg();
    ESP_LOGI(LOG_TAG, "TMC2300: v=%d", iont.version);

    REG_CHOPCONF chopconf = stepperDriver.readChopconf();
    chopconf.setMicrosteps(TMC_MICROSTEPS);
    chopconf.diss2vs = true; // HOTFIX
    chopconf.diss2g = true; // HOTFIX
    stepperDriver.writeChopconfReg(chopconf);

    REG_IHOLD_IRUN iholdIrun;
    iholdIrun.irun = 16;
    iholdIrun.ihold = 1;
    iholdIrun.iholddelay = 1;
    stepperDriver.writeIholdIrunReg(iholdIrun);

    statusPixel.Begin();
    statusPixel.ClearTo(colorBlack);
    statusPixel.Show();
}

bool runStepper() {
    if (stepInterval <= 0) {
	    return false;
    }

    unsigned long time = micros();
    if (time - lastStepTime >= stepInterval) {
        currentSteps += direction;

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

void loop() {
    if (currentSteps != targetSteps) {
        runStepper();
    }
    else if (currentSteps == 0) {
        Serial.println("Opening");
        targetSteps = TMC_OPEN_STEPS;
        direction = DIRECTION_CW;
        digitalWrite(TMC_DIR_PIN, LOW);
        statusPixel.ClearTo(colorYellow);
        statusPixel.Show();
    }
    else {
        Serial.println("Closing");
        targetSteps = 0;
        direction = DIRECTION_CCW;
        digitalWrite(TMC_DIR_PIN, HIGH);
        statusPixel.ClearTo(colorBlue);
        statusPixel.Show();
    }
}