#pragma once

#include "Arduino.h"
#include "config.h"
#include <tmc2300.h>
#include <AccelStepper.h>
#include <ESP32Servo.h>

class Petals {
  public:
    virtual void init(long currentPosition = 0) = 0;
    virtual void update() = 0;

    virtual void setPetalsOpenLevel(uint8_t level, int transitionTime = 0) = 0;
    virtual uint8_t getPetalsOpenLevel() = 0;
    virtual uint8_t getCurrentPetalsOpenLevel() = 0;
    virtual bool arePetalsMoving() = 0;
    virtual bool setEnabled(bool enabled) = 0;
};

class StepperPetals : public Petals {
  public:
    StepperPetals(Config *config);
    void init(long currentPosition = 0);
    void update();

    void setPetalsOpenLevel(uint8_t level, int transitionTime = 0);
    uint8_t getPetalsOpenLevel();
    uint8_t getCurrentPetalsOpenLevel();
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

    void setPetalsOpenLevel(uint8_t level, int transitionTime = 0);
    uint8_t getPetalsOpenLevel();
    uint8_t getCurrentPetalsOpenLevel();
    bool arePetalsMoving();
    bool setEnabled(bool enabled);

  private:
    Config *config;

    // servo config
    Servo servo;

    // servo state
    int8_t petalsOpenLevel; // 0-100% (target angle in percentage)
    unsigned int servoAngle; // current angle
    unsigned int servoOriginAngle; // angle before animation
    unsigned int servoTargetAngle; // angle after animation
    unsigned long movementStartTime;
    unsigned int movementTransitionTime;
    bool enabled;
    unsigned long servoPowerOffTime; // time when servo should power off (after animation is finished)
};