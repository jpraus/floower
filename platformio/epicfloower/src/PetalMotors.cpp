#include "PetalMotors.h"

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "ServoPetals";
#endif

// Setting PWM properties
#define PWM_FREQUENCY 30000
#define PWM_RESOLUTION 8
#define MIN_DUTY_CYCLE 180
#define MAX_DUTY_CYCLE 255

PetalMotors::PetalMotors() : motorMultiplexor(0x20), animations(6) {
}

void PetalMotors::setup() {
    motorMultiplexor.begin(21, 22);
    Serial.println(motorMultiplexor.isConnected());

    setupMotor(0, 14, 0, 1);
    setupMotor(1, 27, 2, 3);
    setupMotor(2, 26, 4, 5);
    setupMotor(3, 25, 6, 7);
    setupMotor(4, 33, 8, 9);
    setupMotor(5, 32, 10, 11);
}

void PetalMotors::setupMotor(uint8_t motorIndex, uint8_t pinEnable, uint8_t portIn1, uint8_t portIn2) {
    pinMode(pinEnable, OUTPUT);
    ledcSetup(motorIndex, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(pinEnable, motorIndex);
    motors[motorIndex] = {pinEnable, motorIndex, portIn1, portIn2, 0, 0, MIN_DUTY_CYCLE};
}

void PetalMotors::update() {
    animations.UpdateAnimations();
}

void PetalMotors::runMotor(uint8_t motorIndex, uint8_t direction, uint16_t durationMillis, uint8_t acceleration) {
    Motor *motor = &motors[motorIndex];
    motor->direction = direction;
    motor->dutyCycle = acceleration > 0 ? MIN_DUTY_CYCLE : MAX_DUTY_CYCLE;
    //motor.dutyCycle = MAX_DUTY_CYCLE;
    motor->acceleration = acceleration;
    motor->durationMillis = durationMillis;

    ledcWrite(motor->pwmChannel, motor->dutyCycle);

    if (direction == DIRECTION_OPEN) {
        ESP_LOGI(LOG_TAG, "Opening %d", motorIndex);
        motorMultiplexor.write(motor->portIn1, LOW);
        motorMultiplexor.write(motor->portIn2, HIGH);
    }
    else {
        ESP_LOGI(LOG_TAG, "Closing %d", motorIndex);
        motorMultiplexor.write(motor->portIn1, HIGH);
        motorMultiplexor.write(motor->portIn2, LOW);
    }

    animations.StartAnimation(motorIndex, durationMillis, [=](const AnimationParam& param){ motorAnimationUpdate(param); });  
}

void PetalMotors::motorAnimationUpdate(const AnimationParam& param) {
    Motor *motor = &motors[param.index];

    if (motor->acceleration > 0) {
        uint16_t dutyCycle = motor->dutyCycle;

        if (param.progress < 0.5 && dutyCycle < 255) { // acceleration
            float durationProgress = motor->durationMillis * param.progress;
            dutyCycle = (durationProgress / motor->acceleration) + MIN_DUTY_CYCLE;
        }
        else if (param.progress > 0.5) { // deceleration
            float progress = 1 - param.progress; // 0.5 -> 0
            float durationProgress = motor->durationMillis * progress; // 10000: 5000 -> 0
            dutyCycle = (durationProgress / motor->acceleration) + MIN_DUTY_CYCLE; // 10: 500 -> 0 (680 -> 180)
        }

        if (dutyCycle > 255) {
            dutyCycle = 255;
        }
        if (dutyCycle != motor->dutyCycle) {
            motor->dutyCycle = dutyCycle;
            ledcWrite(motor->pwmChannel, dutyCycle);
        }
    }

    if (param.state == AnimationState_Completed) {
        ESP_LOGI(LOG_TAG, "Stopping %d", param.index);
        motorMultiplexor.write(motor->portIn1, LOW);
        motorMultiplexor.write(motor->portIn2, LOW);
    }
}

void PetalMotors::stopMotor(uint8_t motorIndex) {
    Motor *motor = &motors[motorIndex];
    animations.StopAnimation(motorIndex);
    ESP_LOGI(LOG_TAG, "Stopping %d", motorIndex);
    motorMultiplexor.write(motor->portIn1, LOW);
    motorMultiplexor.write(motor->portIn2, LOW);
}