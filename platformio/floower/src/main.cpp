#define FIRMWARE_VERSION 8
#define HARDWARE_REVISION 9 // Floower revision 9+ is using stepper motor instead of servo and has a sensor platform available

#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "Calibration.h"
#include "Remote.h"
#include "StateMachine.h"

///////////// SOFTWARE CONFIGURATION

// feature flags
const bool deepSleepEnabled = true;
const bool bluetoothEnabled = true;
const bool colorPickerEnabled = true;
const bool touchEnabled = true;

///////////// HARDWARE CALIBRATION CONFIGURATION
// following constant are used only when Floower is calibrated in factory
// never ever uncomment the CALIBRATE_HARDWARE flag, you will overwrite your hardware calibration settings and probably break the Floower

//#define CALIBRATE_HARDWARE_SERIAL 1
//#define FACTORY_RESET 1
//#define CALIBRATE_HARDWARE 1
#define SERIAL_NUMBER 402

#define WDT_TIMEOUT 10 // 10s for watch dog, reset with ever periodic operation

Config config(FIRMWARE_VERSION);
Floower floower(&config);
Remote remote(&floower, &config);
Calibration calibration(&floower, &config);
StateMachine stateMachine(&config, &floower, &remote);

void configure();
void planDeepSleep(long timeoutMs);
void enterDeepSleep();
void periodicOperation();

void setup() {
    Serial.begin(115200);
    ESP_LOGI(LOG_TAG, "Initializing");

    // start watchdog timer
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
    esp_task_wdt_add(nullptr);

    // read configuration
    configure();
    config.deepSleepEnabled = deepSleepEnabled;
    config.bluetoothEnabled = bluetoothEnabled;
    config.colorPickerEnabled = colorPickerEnabled;
    config.touchEnabled = touchEnabled;

    // after wake up setup
    bool wokeUp = false;
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (config.deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeup_reason) {
        ESP_LOGI(LOG_TAG, "Waking up after Deep Sleep");
        floower.registerOutsideTouch();
        wokeUp = true;
    }

    // init hardware
    //esp_wifi_stop();
    btStop();
    floower.init();
    floower.readPowerState(); // calibrate the ADC
    delay(50); // wait to warm-up

    // init state machine, this is core logic
    stateMachine.init(wokeUp);
}

void loop() {
    floower.update();
    stateMachine.update();

#ifdef CALIBRATE_HARDWARE_SERIAL
    if (!config.calibrated) {
        calibration.calibrate();
    }
#endif

    // save some power when there is nothing happening
    if (stateMachine.isIdle()) {
        delay(10);
    }
}

void configure() {
    config.begin();
#ifdef CALIBRATE_HARDWARE
    config.hardwareCalibration(HARDWARE_REVISION, SERIAL_NUMBER);
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