//#include <esp_wifi.h>
//#include <WiFi.h>
//#include <esp_task_wdt.h>
#include "floower.h"
#include "config.h"
#include "remote.h"

///////////// SOFTWARE CONFIGURATION

#define FIRMWARE_VERSION 2
const bool remoteEnabled = true;
const bool deepSleepEnabled = true;

///////////// HARDWARE CALIBRATION CONFIGURATION
// following constant are used only when Floower is calibrated in factory
// never ever uncomment the CALIBRATE_HARDWARE flag, you will overwrite your hardware calibration settings and probably break the Floower

//#define CALIBRATE_HARDWARE 1
#define SERVO_CLOSED 800 // 650
#define SERVO_OPEN SERVO_CLOSED + 500 // 700
#define SERIAL_NUMBER 32
#define REVISION 6

///////////// POWER MODE

// tuned for 1600mAh LIPO battery
#define POWER_LOW_ENTER_THRESHOLD 0 // enter state when voltage drop below this threshold (3.55)
#define POWER_LOW_LEAVE_THRESHOLD 0 // leave state when voltage rise above this threshold (3.6)
#define POWER_DEAD_THRESHOLD 3.4

///////////// STATE OF FLOWER

#define STATE_INIT 0
#define STATE_WAKEUP 1
#define STATE_STANDBY 2
#define STATE_LIT 3
#define STATE_BLOOMED 4
#define STATE_CLOSED 5

#define STATE_BATTERYDEAD 10
#define STATE_SHUTDOWN 11

byte state = STATE_INIT;
bool colorPickerOn = false;

long changeStateTime = 0;
byte changeStateTo;

long colorsUsed = 0;

///////////// CODE

#define DEEP_SLEEP_INACTIVITY_TIMEOUT 20000 // fall in deep sleep after timeout
#define BATTERY_DEAD_WARNING_DURATION 5000 // how long to show battery dead status
#define PERIODIC_OPERATIONS_INTERVAL 5000

long periodicOperationsTime = 0;
long initRemoteTime = 0;

Config config(FIRMWARE_VERSION);
Floower floower(&config);
Remote remote(&floower, &config);

void setup() {
  Serial.begin(115200);
  ESP_LOGI(LOG_TAG, "Initializing");
  configure();
  changeState(STATE_STANDBY);

  // after wake up setup
  bool wasSleeping = false;
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeup_reason) {
	ESP_LOGI(LOG_TAG, "Waking up after Deep Sleep");
    floower.touchISR();
    wasSleeping = true;
  }

  // init hardware
  //esp_wifi_stop();
  //btStop();
  floower.init();
  floower.readBatteryState(); // calibrate the ADC
  floower.onLeafTouch(onLeafTouch);
  Floower::touchAttachInterruptProxy([](){ floower.touchISR(); });
  delay(50); // wait for init

  // check if there is enough power to run
  bool isBatteryDead = false;
  if (!floower.isUSBPowered()) {
    Battery battery = floower.readBatteryState();
    if (battery.voltage < POWER_DEAD_THRESHOLD) {
      delay(500);
      battery = floower.readBatteryState(); // re-verify the voltage after .5s
      isBatteryDead = battery.voltage < POWER_DEAD_THRESHOLD;
    }
  }

  if (isBatteryDead) {
    // battery is dead, do not wake up, shutdown after a status color
	ESP_LOGW(LOG_TAG, "Battery is dead, shutting down");
    changeState(STATE_BATTERYDEAD);
    planChangeState(STATE_SHUTDOWN, BATTERY_DEAD_WARNING_DURATION);
    floower.setLowPowerMode(true);
    floower.setColor(colorRed, FloowerColorMode::PULSE, 1000);
  }
  else {
    // normal operation
    floower.initServo();
    if (!wasSleeping) {
      floower.setPetalsOpenLevel(0, 100);
    }
  }

  if (remoteEnabled) {
    initRemoteTime = millis() + 5000; // defer init of BLE by 5 seconds
  }

  periodicOperationsTime = millis() + PERIODIC_OPERATIONS_INTERVAL; // TODO millis overflow
  ESP_LOGI(LOG_TAG, "Ready");
}

void loop() {
  floower.update();
  remote.update();

  // timers
  long now = millis();
  if (periodicOperationsTime > 0 && periodicOperationsTime < now) {
    periodicOperationsTime = now + PERIODIC_OPERATIONS_INTERVAL;
    periodicOperation();
  }
  if (changeStateTime > 0 && changeStateTime < now) {
    changeStateTime = 0;
    changeState(changeStateTo);
  }
  if (initRemoteTime > 0 && initRemoteTime < now) {
    initRemoteTime = 0;
    remote.init();
  }

  // update state machine
  if (state == STATE_WAKEUP) {
    floower.setColor(nextRandomColor(), FloowerColorMode::TRANSITION, 5000);
    changeState(STATE_LIT);
  }
  else if (state == STATE_SHUTDOWN && !floower.arePetalsMoving()) {
    enterDeepSleep();
  }

  // save some power when flower is idle
  if (floower.isIdle()) {
    delay(10);
  }
}

void periodicOperation() {
  floower.acty();
  powerWatchDog();
}

void planChangeState(byte newState, long timeout) {
  changeStateTo = newState;
  changeStateTime = millis() + timeout;
  ESP_LOGD(LOG_TAG, "Planned change state to %d in %d", newState, timeout);
}

void changeState(byte newState) {
  changeStateTime = 0;
  if (state != newState) {
    state = newState;
    ESP_LOGD(LOG_TAG, "Changed state to %d", newState);

    if (newState == STATE_STANDBY && deepSleepEnabled && !remoteEnabled) {
      planChangeState(STATE_SHUTDOWN, DEEP_SLEEP_INACTIVITY_TIMEOUT);
    }
  }
}

void onLeafTouch(FloowerTouchType touchType) {
  if (state == STATE_BATTERYDEAD || state == STATE_SHUTDOWN) {
    return;
  }

  switch (touchType) {
    case RELEASE:
      if (colorPickerOn) {
        floower.stopColorPicker();
        colorPickerOn = false;
      }
      else if (floower.isIdle()) {
        if (state == STATE_STANDBY) {
          // open + set color
          floower.setColor(nextRandomColor(), FloowerColorMode::TRANSITION, 5000);
          floower.setPetalsOpenLevel(100, 5000);
          changeState(STATE_BLOOMED);
        }
        else if (state == STATE_LIT) {
          // open
          floower.setPetalsOpenLevel(100, 5000);
          changeState(STATE_BLOOMED);
        }
        else if (state == STATE_BLOOMED) {
          // close
          floower.setPetalsOpenLevel(0, 5000);
          changeState(STATE_CLOSED);
        }
        else if (state == STATE_CLOSED) {
          // shutdown
          floower.setColor(colorBlack, FloowerColorMode::TRANSITION, 2000);
          changeState(STATE_STANDBY);
        }
      }
      break;

    case HOLD:
      floower.startColorPicker();
      colorPickerOn = true;
      if (state == STATE_STANDBY) {
        changeState(STATE_LIT);
      }
      break;
  }
}

void powerWatchDog() {
  if (state == STATE_BATTERYDEAD || state == STATE_SHUTDOWN) {
    return;
  }

  bool charging = floower.isUSBPowered();
  Battery battery = floower.readBatteryState();

  remote.setBatteryLevel(battery.level, charging);

  if (charging) {
    floower.setLowPowerMode(false);
  }
  else if (battery.voltage < POWER_DEAD_THRESHOLD) {
	ESP_LOGW(LOG_TAG, "Shutting down, battery is dead (%dV)", battery.voltage);
    floower.setColor(colorBlack, FloowerColorMode::TRANSITION, 2500);
    floower.setPetalsOpenLevel(0, 2500);
    changeState(STATE_SHUTDOWN);
  }
  else if (!floower.isLowPowerMode() && battery.voltage < POWER_LOW_ENTER_THRESHOLD) {
	ESP_LOGI(LOG_TAG, "Entering low power mode (%dV)", battery.voltage);
    floower.setLowPowerMode(true);
  }
  else if (floower.isLowPowerMode() && battery.voltage >= POWER_LOW_LEAVE_THRESHOLD) {
	ESP_LOGI(LOG_TAG, "Leaving low power mode (%dV)", battery.voltage);
    floower.setLowPowerMode(false);
  }
}

void enterDeepSleep() {
  // TODO: move to floower class
  touchAttachInterrupt(4, [](){}, 50); // register interrupt to enable wakeup
  esp_sleep_enable_touchpad_wakeup();
  ESP_LOGI(LOG_TAG, "Going to sleep now");
  //esp_wifi_stop();
  btStop();
  esp_deep_sleep_start();
}

RgbColor nextRandomColor() {
  if (colorsUsed > 0) {
    long maxColors = pow(2, config.colorSchemeSize) - 1;
    if (maxColors == colorsUsed) {
      colorsUsed = 0; // all colors used, reset
    }
  }

  byte colorIndex;
  long colorCode;
  int maxIterations = config.colorSchemeSize * 3;

  do {
    colorIndex = random(0, config.colorSchemeSize);
    colorCode = 1 << colorIndex;
    maxIterations--;
  } while ((colorsUsed & colorCode) > 0 && maxIterations > 0); // already used before all the rest colors

  colorsUsed += colorCode;
  return config.colorScheme[colorIndex];
}

// configuration

void configure() {
  config.begin();
#ifdef CALIBRATE_HARDWARE
  config.hardwareCalibration(SERVO_CLOSED, SERVO_OPEN, REVISION, SERIAL_NUMBER);
  config.commit();
#endif
  config.load();
}
