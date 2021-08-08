#pragma once

#include "Arduino.h"
#include "Config.h"
#include <tmc2300.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>

class Petals {
    public:
        virtual void init(long currentPosition = 0) = 0;
        virtual void update() = 0;

        virtual void setPetalsOpenLevel(int8_t level, int transitionTime = 0) = 0; // level need to be signed to compare with local signed variable
        virtual int8_t getPetalsOpenLevel() = 0;
        virtual int8_t getCurrentPetalsOpenLevel() = 0;
        virtual bool arePetalsMoving() = 0;
        virtual bool setEnabled(bool enabled) = 0;
};

class StepperPetals : public Petals {
    public:
        StepperPetals(Config *config);
        void init(long currentPosition = 0);
        void update();

        void setPetalsOpenLevel(int8_t level, int transitionTime = 0);
        int8_t getPetalsOpenLevel();
        int8_t getCurrentPetalsOpenLevel();
        bool arePetalsMoving();
        bool setEnabled(bool enabled);

    private:
        Config *config;

        // stepper config
        TMC2300 stepperDriver;
        AccelStepper stepperMotion;

        // stepper state
        int8_t petalsOpenLevel; // 0-100% (target angle in percentage)
        bool enabled;
};

class ServoPetals : public Petals {
    public:
        ServoPetals(Config *config);
        void init(long currentPosition = 0);
        void update();

        void setPetalsOpenLevel(int8_t level, int transitionTime = 0);
        int8_t getPetalsOpenLevel();
        int8_t getCurrentPetalsOpenLevel();
        bool arePetalsMoving();
        bool setEnabled(bool enabled);

    private:
        Config *config;

        // servo config
        Servo servo;

        // servo state
        int8_t petalsOpenLevel; // 0-100% (target angle in percentage)
        int16_t servoAngle; // current angle, keep signed to be able to calculate closing
        int16_t servoOriginAngle; // angle before animation, keep signed to be able to calculate closing
        int16_t servoTargetAngle; // angle after animation, keep signed to be able to calculate closing
        unsigned long movementStartTime;
        uint16_t movementTransitionTime;
        bool enabled;
        unsigned long servoPowerOffTime; // time when servo should power off (after animation is finished)
};