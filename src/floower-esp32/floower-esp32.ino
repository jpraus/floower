//#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include "floower.h"
#include "config.h"
#include "automaton.h"
#include "remote.h"

///////////// SOFTWARE CONFIGURATION

#define FIRMWARE_VERSION 8

// feature flags
const bool deepSleepEnabled = true;
const bool bluetoothEnabled = true;
const bool rainbowEnabled = true;
const bool touchEnabled = true;

///////////// HARDWARE CALIBRATION CONFIGURATION
// following constant are used only when Floower is calibrated in factory
// never ever uncomment the CALIBRATE_HARDWARE flag, you will overwrite your hardware calibration settings and probably break the Floower

//#define CALIBRATE_HARDWARE_SERIAL 1
//#define FACTORY_RESET 1
//#define CALIBRATE_HARDWARE 1
#define SERIAL_NUMBER 400
#define REVISION 9

///////////// POWER MODE

// tuned for 1600mAh LIPO battery
#define POWER_LOW_ENTER_THRESHOLD 0 // enter state when voltage drop below this threshold (3.55)
#define POWER_LOW_LEAVE_THRESHOLD 0 // leave state when voltage rise above this threshold (3.6)
#define POWER_DEAD_THRESHOLD 3.4

///////////// CODE

#define REMOTE_INIT_TIMEOUT 2000 // delay init of BLE to lower the power surge on startup
#define DEEP_SLEEP_INACTIVITY_TIMEOUT 60000 // fall in deep sleep after timeout
#define BATTERY_DEAD_WARNING_DURATION 5000 // how long to show battery dead status
#define PERIODIC_OPERATIONS_INTERVAL 3000
#define WDT_TIMEOUT 10 // 10s for watch dog, reset with ever periodic operation

bool batteryDead = false;
bool usbPowered = false;
bool switchedOn = false;
long deepSleepTime = 0;
long periodicOperationsTime = 0;
long initRemoteTime = 0;

Config config(FIRMWARE_VERSION);
Floower floower(&config);
Remote remote(&floower, &config);
Automaton automaton(&remote, &floower, &config);

void setup() {
  Serial.begin(115200);
  ESP_LOGI(LOG_TAG, "Initializing");

  // start watchdog timer
  esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);

  // read configuration
  configure();
  config.bluetoothEnabled = bluetoothEnabled;
  config.rainbowEnabled = rainbowEnabled;
  config.touchEnabled = touchEnabled;

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
  btStop();
  floower.init();
  floower.readPowerState(); // calibrate the ADC
  delay(50); // wait to warm-up

  // check if there is enough power to run
  PowerState powerState = floower.readPowerState();
  if (!powerState.usbPowered && powerState.batteryVoltage < POWER_DEAD_THRESHOLD) {
    delay(500);
    powerState = floower.readPowerState(); // re-verify the voltage after .5s
    batteryDead = powerState.batteryVoltage < POWER_DEAD_THRESHOLD;
  }

  if (batteryDead) {
    // battery is dead, do not wake up, shutdown after a status color
	  ESP_LOGW(LOG_TAG, "Battery is dead, shutting down");
    planDeepSleep(BATTERY_DEAD_WARNING_DURATION);
    floower.setLowPowerMode(true);
    floower.flashColor(colorRed.H, colorRed.S, 1000);
  }
  else if (!config.calibrated) {
    // hardware is not calibrated
    ESP_LOGW(LOG_TAG, "Floower not calibrated");
    floower.flashColor(colorPurple.H, colorPurple.S, 2000);
    floower.initStepper(35000); // open steps, force the Floower to close
    ESP_LOGI(LOG_TAG, "Ready for calibration");
  }
  else if (config.hardwareRevision < 9) {
    // incompatible hardware
    ESP_LOGW(LOG_TAG, "Floower is not compatible");
    floower.flashColor(colorRed.H, colorRed.S, 2000);
  }
  else {
    // normal operation
    floower.initStepper();
    floower.enableTouch(!wasSleeping);
    switchedOn = powerState.switchedOn;
    if (switchedOn) {
      automaton.init();
      if (config.initRemoteOnStartup) {
        initRemoteTime = millis() + REMOTE_INIT_TIMEOUT; // defer init of BLE by 5 seconds
      }
    }
    else {
      ESP_LOGI(LOG_TAG, "Power switch is OFF");
    }
    ESP_LOGI(LOG_TAG, "Ready");
  }

  periodicOperationsTime = millis() + PERIODIC_OPERATIONS_INTERVAL; // TODO millis overflow
}

void loop() {
  floower.update();
  automaton.update();

  // timers
  long now = millis();
  if (periodicOperationsTime != 0 && periodicOperationsTime < now) {
    periodicOperationsTime = now + PERIODIC_OPERATIONS_INTERVAL;
    periodicOperation();
  }
  if (config.bluetoothEnabled && initRemoteTime != 0 && initRemoteTime < now && !floower.arePetalsMoving()) {
    initRemoteTime = 0;
    remote.init();
    remote.startAdvertising();
  }
  if (deepSleepTime != 0 && deepSleepTime < now && !floower.arePetalsMoving()) {
    deepSleepTime = 0;
    enterDeepSleep();
  }
  if (remote.isConnected()) {
    floower.acty();
  }

  // plan to enter deep sleep in inactivity
  if (deepSleepEnabled && !batteryDead) {
    // plan to enter deep sleep to save power if floower is in open/dark position & remote is not connected
    if (!usbPowered && !floower.isLit() && !floower.arePetalsMoving() && floower.getPetalsOpenLevel() == 0 && !remote.isConnected()) {
      if (deepSleepTime == 0) {
        planDeepSleep(DEEP_SLEEP_INACTIVITY_TIMEOUT);
      }
    }
    else if (deepSleepTime != 0) {
      ESP_LOGI(LOG_TAG, "Sleep disabled");
      deepSleepTime = 0;
    }
  }

#ifdef CALIBRATE_HARDWARE_SERIAL
  if (!config.calibrated) {
    calibrateOverSerial();
  }
#endif

  // save some power when flower is not animating
  if (!floower.isAnimating()) {
    delay(10);
  }
}

void periodicOperation() {
  esp_task_wdt_reset();
  floower.acty();
  powerWatchDog();
}

void powerWatchDog() {
  if (batteryDead) {
    return;
  }

  PowerState powerState = floower.readPowerState();
  remote.setBatteryLevel(powerState.batteryLevel, powerState.batteryCharging);
  usbPowered = powerState.usbPowered;

  if (usbPowered) {
    floower.setLowPowerMode(false);
    if (powerState.switchedOn != switchedOn) {
      switchedOn = powerState.switchedOn;
      if (switchedOn) {
        ESP_LOGI(LOG_TAG, "Switched ON");
        automaton.init();
        if (config.initRemoteOnStartup) {
          initRemoteTime = millis();
        }
      }
      else {
        ESP_LOGI(LOG_TAG, "Switched OFF");
        automaton.suspend();
      }
    }
  }
  else if (powerState.batteryVoltage < POWER_DEAD_THRESHOLD) {
    ESP_LOGW(LOG_TAG, "Shutting down, battery is dead (%dV)", powerState.batteryVoltage);
    floower.setLowPowerMode(true);
    floower.flashColor(colorRed.H, colorRed.S, 1000);
    floower.setPetalsOpenLevel(0, 2500);
    planDeepSleep(BATTERY_DEAD_WARNING_DURATION);
    batteryDead = true;
  }
  else if (!floower.isLowPowerMode() && powerState.batteryVoltage < POWER_LOW_ENTER_THRESHOLD) {
    ESP_LOGI(LOG_TAG, "Entering low power mode (%dV)", powerState.batteryVoltage);
    floower.setLowPowerMode(true);
  }
  else if (floower.isLowPowerMode() && powerState.batteryVoltage >= POWER_LOW_LEAVE_THRESHOLD) {
    ESP_LOGI(LOG_TAG, "Leaving low power mode (%dV)", powerState.batteryVoltage);
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
  config.hardwareCalibration(REVISION, SERIAL_NUMBER);
  config.factorySettings();
  config.setCalibrated();
  config.commit();
#endif
#ifdef FACTORY_RESET
  config.factorySettings();
  config.commit();
#endif
  config.load();
}

#ifdef CALIBRATE_HARDWARE_SERIAL

uint8_t calibationCommand = 0;
String calibrationValue;

void calibrateOverSerial() {
  if (Serial.available() > 0) {
    char data = Serial.read();
    if (calibationCommand == 0) { // start of command
      calibationCommand = data;
      calibrationValue = "";
    }
    else if (data == '\n'){ // end of command
      long value = calibrationValue.toInt();
      if (calibationCommand == 'N') { // serial number
        ESP_LOGI(LOG_TAG, "New S/N %d", value);
        config.serialNumber = value;
      }
      else if (calibationCommand == 'H') { // hardware revision
        ESP_LOGI(LOG_TAG, "New HW revision %d", value);
        config.hardwareRevision = value;
      }
      else if (calibationCommand == 'E') { // end of calibration
        config.hardwareCalibration(config.servoClosed, config.servoOpen, config.hardwareRevision, config.serialNumber);
        config.factorySettings();
        config.setCalibrated();
        config.commit();
        ESP_LOGI(LOG_TAG, "Calibration done");
        ESP.restart(); // restart now
      }
      calibationCommand = 0;
    }
    else { // command value
      calibrationValue += data;
    }
  }
}

#endif
