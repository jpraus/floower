#ifndef PETALS_H
#define PETALS_H

#include "Arduino.h"
#include "config.h"
#include <tmc2300.h>
#include <AccelStepper.h>

class Petals {
  public:
    Petals(Config *config);
    void init();
    void initStepper(long currentPosition = 0);
    void update();

    void setPetalsOpenLevel(uint8_t level, int transitionTime = 0);
    uint8_t getPetalsOpenLevel();
    uint8_t getCurrentPetalsOpenLevel();
    bool arePetalsMoving();

  private:
    bool setStepperPowerOn(bool powerOn);

    Config *config;

    // stepper config
    TMC2300 stepperDriver;
    AccelStepper stepperMotion;

    // stepper state
    int8_t petalsOpenLevel; // 0-100% (target angle in percentage)
    bool stepperPowerOn;
};

#endif
