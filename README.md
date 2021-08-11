# [![Floower](https://floower.io/wp-content/uploads/2020/04/floower.png)](https://floower.io)

## Floower is a nature-inspired smart connected device with open-source software that blooms and shines on wish.

Floower can be connected to home assistants, sensors, and other smart devices and create a very original way how to interface with humans. Smart devices don't have to be plastic, square, and ugly. They can be as beautiful and intricate as Floower. It's a smart art.

Visit [floower.io](https://floower.io) to learn more.

[![This is Floower](https://github.com/jpraus/floower/blob/master/doc/floower-preview.jpg?raw=true)](https://www.youtube.com/watch?v=r68Je8jlLY4)

## Features

* 6 petals that open and close - operated by stepper motor inside the flowerpot
* 7 individually addressable RGB LEDs in the center of the blossom
* capacitive touch sensor in the form of leaf
* ESP32 microcontroller
* RTC clock IC
* Bluetooth & WiFi connectivity
* RGB status LED in the floowerpot
* 1600 mAh LIPO battery (TP4056 charging IC)
* ultra low power mode to extend battery life
* USB-C interface for programming (CP2102N USB-to-UART)
* PlatformIO or MicroPython support
* extendable with I2C sensors

## How do I Get One?

Visit [floower.io](https://floower.io).

## Understanding Floower Hardware

### Logic Boards History

* **Revision 5**
  * Serial numbers 0001 - 0018, 0021, 0022
  * The history starts here
* **Revision 6**
  * Serial numbers 0019, 0020, 0023 - 0132
  * Added USB power detection circuitry
* **Revision 7**
  * Serial numbers 0133 +
  * Ability to charge from quick/smart charges
* **Revision 8**
* **Revision 9**  

## Upgrading the Flooware

The Floower firmware is called Flooware. We aim to provide all the Floowers with the latest updates to get the latest features and new seasonal updates. The new age of Smart Art has just begun! [Upgrade your Floower with latest Flooware](https://floower.io/upgrading-floower/).

## Customizing the Floower code

I would love to build a community of developers around Floower. I would love to have you write custom mods to the behavior of Floower to bring joy into everyday life. That’s my dream. After all, it’s an Opensource software and every Floower is equipped with a USB-C interface to flash it with the firmware. Just hook it up to your computer and you are good to go!

Customizing the Floower is fun. I already made several mods for several occasions and faires – Halloween Floower, color mixer, blooming garden installation, simple reflex game, etc. And it nicely shows that the Floower is a very versatile device. Any bug-fixes, improvements, or new features are welcomed! Follow up on the next steps to get your IDE setup for Flooware development.

### Prerequisites

All you need to have is [Visual Studio Code](https://code.visualstudio.com/) and [PlatformIO plugin](https://platformio.org/). That's it! Do it.

### Compiling and uploading the code

* Download / Clone this code repository

![Code repository](https://github.com/jpraus/floower/blob/master/doc/github-code.png?raw=true)

* Open platformio/floower project with PlatformIO
* Connect your Floower to your computer via USB-C data cable. Make sure you are using smart/data USB cable, some of the cables are power only and thus cannot transfer data at all.
* Compile and upload the code.
* Read a [Quickstart Guide](https://floower.io/i-wrote-a-terrible-firmware/) on Floower's blog to learn some details about the code architecture and classes.
* Have a fun!
