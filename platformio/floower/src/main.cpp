#define FIRMWARE_VERSION 9
#define HARDWARE_REVISION 9 // Floower revision 9+ is using stepper motor instead of servo and has a sensor platform available

#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include "Config.h"
#include "connect/RemoteControl.h"
#include "behavior/BloomingBehavior.h"
#include "behavior/MindfulnessBehavior.h"
#include "behavior/Calibration.h"

///////////// SOFTWARE CONFIGURATION

// feature flags
void setFeatureFlags(Config &config) {
    config.deepSleepEnabled = true;
    config.bluetoothEnabled = true;
    config.wifiEnabled = true;
    config.colorPickerEnabled = true;
}

///////////// HARDWARE CALIBRATION CONFIGURATION
// following constant are used only when Floower is calibrated in factory
// never ever uncomment the CALIBRATE_HARDWARE flag, you will overwrite your hardware calibration settings and probably break the Floower

//#define CALIBRATE_HARDWARE_SERIAL 1
//#define FACTORY_RESET 1
//#define CALIBRATE_HARDWARE 1
//#define SERVO_CLOSED 800 // 650
//#define SERVO_OPEN SERVO_CLOSED + 500 // 700
//#define SERIAL_NUMBER 402

#define WDT_TIMEOUT 10 // 10s for watch dog, reset with ever periodic operation

Config config(FIRMWARE_VERSION);
Floower floower(&config);
Behavior *behavior;

CommandProtocol cmdProtocol(&config, &floower);
BluetoothConnect bluetoothConnect(&floower, &config, &cmdProtocol);
WifiConnect wifiConnect(&config, &cmdProtocol);
RemoteControl remoteControl(&bluetoothConnect, &wifiConnect, &cmdProtocol);

void configure();
void planDeepSleep(long timeoutMs);
void enterDeepSleep();
void periodicOperation();

void onConfigChanged(bool wifiChanged) {
    ESP_LOGI(LOG_TAG, "Config changed: wifi=%d", wifiChanged ? 1 : 0);
    if (wifiChanged) {
        wifiConnect.reconnect();
    }
    bluetoothConnect.reloadConfig();
}

void onFloowerChanged(int8_t petalsOpenLevel, HsbColor hsbColor) {
    ESP_LOGI(LOG_TAG, "Floower state changed");
    wifiConnect.updateFloowerState(petalsOpenLevel, hsbColor);
    bluetoothConnect.updateFloowerState(petalsOpenLevel, hsbColor);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    ESP_LOGI(LOG_TAG, "Initializing");

    // start watchdog timer
    esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
    esp_task_wdt_add(nullptr);

    // read configuration
    configure();
    setFeatureFlags(config);
    config.onConfigChanged(onConfigChanged);

    // after wake up setup
    bool wokeUp = false;
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
    if (config.deepSleepEnabled && ESP_SLEEP_WAKEUP_TOUCHPAD == wakeupReason) {
        ESP_LOGI(LOG_TAG, "Waking up (touch)");
        floower.registerOutsideTouch();
        wokeUp = true;
    }
    else if (ESP_SLEEP_WAKEUP_EXT0 == wakeupReason) {
        ESP_LOGI(LOG_TAG, "Waking up (GPIO)");
        wokeUp = true;
    }

    // init hardware
    //esp_wifi_stop();
    btStop();
    floower.init();
    floower.enableTouch([=](FloowerTouchEvent event){}, !wokeUp); // enable NOP touch to enable deep sleep wake up function
    floower.readPowerState(); // calibrate the ADC
    floower.onChange(onFloowerChanged);
    delay(50); // wait to warm-up

    // init state machine, this is core logic
    if (!config.calibrated || !config.touchCalibrated) {
        behavior = new Calibration(&config, &floower, &bluetoothConnect, config.calibrated && !config.touchCalibrated);
    }
    else {
        behavior = new BloomingBehavior(&config, &floower, &remoteControl);
        //behavior = new MindfulnessBehavior(&config, &floower, &remoteControl);
        //behavior = new TestBehavior(&config, &floower, &remoteControl);
    }
    behavior->setup(wokeUp);

    //config.setWifi("", "");
    //config.setFloud("nejbezpecnejsi");
    //config.commit();
}

void loop() {
    floower.update();
    behavior->loop();
    wifiConnect.loop();

    // save some power when there is nothing happening
    if (behavior->isIdle()) {
        delay(10);
    }
}

void configure() {
    config.begin();
#ifdef CALIBRATE_HARDWARE
    config.hardwareCalibration(SERVO_CLOSED, SERVO_OPEN, HARDWARE_REVISION, SERIAL_NUMBER);
    config.factorySettings();
    config.setCalibrated();
    config.commit();
#endif
#ifdef FACTORY_RESET
    config.factorySettings();
    config.setCalibrated();
    config.commit();
#endif
    config.load();
}