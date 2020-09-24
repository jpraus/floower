//#include <esp_wifi.h>
//#include <WiFi.h>
//#include <esp_task_wdt.h>
#include "floower.h"
#include "config.h"
#include "automaton.h"
#include "remote.h"

///////////// SOFTWARE CONFIGURATION

#define FIRMWARE_VERSION 2
const bool remoteEnabled = false;
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

///////////// CODE

#define REMOTE_INIT_TIMEOUT 2000 // delay init of BLE to lower the power surge on startup
#define DEEP_SLEEP_INACTIVITY_TIMEOUT 20000 // fall in deep sleep after timeout
#define BATTERY_DEAD_WARNING_DURATION 5000 // how long to show battery dead status
#define PERIODIC_OPERATIONS_INTERVAL 5000

bool batteryDead = false;
long deepSleepTime = 0;
long periodicOperationsTime = 0;
long initRemoteTime = 0;

Config config(FIRMWARE_VERSION);
Floower floower(&config);
Automaton automaton(&floower, &config);
Remote remote(&floower, &config);

void setup() {
  Serial.begin(115200);
  ESP_LOGI(LOG_TAG, "Initializing");
  configure();

  // after wake up setup
  bool wasSleeping = false;
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeup_reason) {
	  ESP_LOGI(LOG_TAG, "Waking up after Deep Sleep");
    floower.registerOutsideTouch();
    wasSleeping = true;
  }

  // init hardware
  //esp_wifi_stop();
  //btStop();
  floower.init();
  floower.readBatteryState(); // calibrate the ADC
  delay(50); // wait to warm-uo

  // check if there is enough power to run
  if (!floower.isUSBPowered()) {
    Battery battery = floower.readBatteryState();
    if (battery.voltage < POWER_DEAD_THRESHOLD) {
      delay(500);
      battery = floower.readBatteryState(); // re-verify the voltage after .5s
      batteryDead = battery.voltage < POWER_DEAD_THRESHOLD;
    }
  }

  if (batteryDead) {
    // battery is dead, do not wake up, shutdown after a status color
	  ESP_LOGW(LOG_TAG, "Battery is dead, shutting down");
    planDeepSleep(BATTERY_DEAD_WARNING_DURATION);
    floower.setLowPowerMode(true);
    floower.setColor(colorRed, FloowerColorMode::PULSE, 1000);
  }
  else {
    // normal operation
    floower.initServo();
    if (!wasSleeping) {
      floower.setPetalsOpenLevel(0, 100); // reset petals position to known one
    }
    if (remoteEnabled) {
      initRemoteTime = millis() + REMOTE_INIT_TIMEOUT; // defer init of BLE by 5 seconds
    }

    automaton.init();
    periodicOperationsTime = millis() + PERIODIC_OPERATIONS_INTERVAL; // TODO millis overflow
    ESP_LOGI(LOG_TAG, "Ready");
  }
}

void loop() {
  floower.update();
  automaton.update();
  remote.update();

  // timers
  long now = millis();
  if (periodicOperationsTime != 0 && periodicOperationsTime < now) {
    periodicOperationsTime = now + PERIODIC_OPERATIONS_INTERVAL;
    periodicOperation();
  }
  if (initRemoteTime != 0 && initRemoteTime < now) {
    initRemoteTime = 0;
    remote.init();
  }
  if (deepSleepTime != 0 && deepSleepTime < now && !floower.arePetalsMoving()) {
    deepSleepTime = 0;
    enterDeepSleep();
  }

  // plan to enter deep sleep in inactivity
  if (deepSleepEnabled && !batteryDead) {
    if (automaton.canEnterDeepSleep() && remote.canEnterDeepSleep()) {
      if (deepSleepTime == 0) {
        planDeepSleep(DEEP_SLEEP_INACTIVITY_TIMEOUT);
      }
    }
    else if (deepSleepTime != 0) {
      deepSleepTime = 0;
    }
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

void powerWatchDog() {
  if (batteryDead) {
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
    planDeepSleep(0);
    batteryDead = true;
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

void planDeepSleep(long timeoutMs) {
  deepSleepTime = millis() + timeoutMs;
  ESP_LOGI(LOG_TAG, "Sleep in %d", timeoutMs);
}

void enterDeepSleep() {
  ESP_LOGI(LOG_TAG, "Going to sleep now");
  esp_sleep_enable_touchpad_wakeup();
  //esp_wifi_stop();
  btStop();
  esp_deep_sleep_start();
}

void configure() {
  config.begin();
#ifdef CALIBRATE_HARDWARE
  config.hardwareCalibration(SERVO_CLOSED, SERVO_OPEN, REVISION, SERIAL_NUMBER);
  config.commit();
#endif
  config.load();
}
