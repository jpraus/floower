# [![Floower](https://floower.io/wp-content/uploads/2020/04/floower.png)](https://floower.io)

The most advanced flower in the world. Electrical & Mechanical flower lamp that blooms with all the colors of rainbow thanks to the set of RGB LEDs.

[floower.io](https://floower.io)

![Everblooming Flowers](https://github.com/jpraus/floower/blob/master/doc/floower.jpg?raw=true)

## Features

* 6 petals that open and close - operated by servo motor inside the flowerpot
* 7 RGB LEDs in the center of the blossom
* capacitive touch sensor in the leaf
* ESP32 microcontroller
* Arduino IDE or MicroPython support
* Bluetooth & WiFi connectivity
* USB-C interface for programming (CP2102N USB-to-UART)
* 1600 mAh LIPO battery (TP4056 charging IC)
* deep sleep mode to extend battery life
* red charging and blue activity LEDs in the flowerpot

## How do I get one?

Currently prototypes available on [floower.io](https://floower.io). Join waitlist to get notified every time the new batch is ready.

## Understanding Floower hardware

...

### Logic Boards History

* **rev5**
  * Serial numbers 0001 - 0011
  * The history starts here
* **rev6**
  * Added USB power detection circuitry

## Connecting flower to the computer

### Installing 

Floower can be programmed using the Arduino IDE. However, the ESP32 microcontroller is not supported by default, so you need to install & configure it:

* Prerequisites: Arduino IDE Installed
* Install [CP2102 Universal Driver](https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers)
* Install ESP32 Add-on in Arduino IDE
* Open **File > Preferences**
* Enter **https://dl.espressif.com/dl/package_esp32_index.json** into the **Additional Board Manager URLs** field and click **OK** button. (If you already have other boards URL, separate the URLs with a comma)

![Additional Board Manager URLs](https://github.com/jpraus/floower/blob/master/doc/arduino-ide-preferences.png?raw=true)

* Open **Tools > Boards > Boards Manager...** and search for esp32
* Click Install button for the **ESP32 by Espressif Systems**. It should be installed after a few seconds.

![Installing ESP32 by Espressif Systems](https://github.com/jpraus/floower/blob/master/doc/arduino-ide-boards-manager.png?raw=true)

* Open **Tools > Boards** and select **DOIT ESP32 DEVKIT V1**
* That's it!

![Selecting DOIT ESP32 DEVKIT V1 board](https://github.com/jpraus/floower/blob/master/doc/arduino-ide-board.png?raw=true)

### First upload

Checkout this repository and open [flower-esp32.ino](src/flower-esp32/flower-esp32.ino) project file. Connect your Floower to your computer via USB-C data cable. Make sure you are using smart/data USB cable, some of the cables are power only and thus cannot transfer data at all. Select the port of your Floower, it should be the one that just appeared.

Run **Sketch > Upload**. You should get a sweet *'avrdude done. Thank you.'* message at the end of the console. Now you can make your own personalized Floower.

![Arduino IDE upload](https://github.com/jpraus/floower/blob/master/doc/arduino-ide-upload.png?raw=true)
