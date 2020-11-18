#!/usr/bin/python

import RPi.GPIO as GPIO
import Adafruit_CharLCD as LCD
import serial
import serial.tools.list_ports as prtlst
import os
from time import sleep

VERSION = 5

# Raspberry Pi pin configuration:
lcd_rs        = 7  # Note this might need to be changed to 21 for older revision Pi's.
lcd_en        = 8
lcd_d4        = 25
lcd_d5        = 24
lcd_d6        = 23
lcd_d7        = 18
lcd_backlight = 4

# Define LCD column and row size for 16x2 LCD.
lcd_columns = 16
lcd_rows    = 2

# rotary encoder
enc_a = 27
enc_b = 22
enc_sw = 17
enc_state = '00'
enc_direction = None

# Initialize the LCD using the pins above.
lcd = LCD.Adafruit_CharLCD(lcd_rs, lcd_en, lcd_d4, lcd_d5, lcd_d6, lcd_d7, lcd_columns, lcd_rows, lcd_backlight)
serial_connection = None
connected_device = None

# screens
# 0 - connect Floower
# 1 - close calibration
# 2 - open calibration
# 3 - verify open/close
# 4 - serial number
# 5 - hw revision
# 6 - confirm
SCREEN_CONNECT = 0
SCREEN_MENU = 1
SCREEN_CAL_CLOSE = 2
SCREEN_CAL_OPEN = 3
SCREEN_VERIFY = 4
SCREEN_SN = 5
SCREEN_HW_REVISION = 6
SCREEN_CONFIRM = 7
SCREEN_DISCONNECT = 8
SCREEN_FLASH = 9

serial_number = 130
hw_revision = 7
close_value = 0
open_value = 0
screen = 0
screen_option = 0


def initGPIO():
    GPIO.setwarnings(True)
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(enc_a, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    GPIO.setup(enc_b, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    GPIO.setup(enc_sw, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
    GPIO.add_event_detect(enc_a, GPIO.BOTH, callback=encoder_decode)
    GPIO.add_event_detect(enc_b, GPIO.BOTH, callback=encoder_decode)
    GPIO.add_event_detect(enc_sw, GPIO.RISING, callback=button_pushed)
    return


# https://github.com/nstansby/rpi-rotary-encoder-python
def encoder_decode(channel):
    global enc_state, enc_direction

    p1 = GPIO.input(enc_a)
    p2 = GPIO.input(enc_b)
    new_state = "{}{}".format(p1, p2)

    if enc_state == "00":  # Resting position
        if new_state == "01":  # Turned right 1
            enc_direction = "R"
        elif new_state == "10":  # Turned left 1
            enc_direction = "L"

    elif enc_state == "01":  # R1 or L3 position
        if new_state == "11":  # Turned right 1
            enc_direction = "R"
        elif new_state == "00":  # Turned left 1
            if enc_direction == "L":
                encoder_rorated_down()

    elif enc_state == "10":  # R3 or L1
        if new_state == "11":  # Turned left 1
            enc_direction = "L"
        elif new_state == "00":  # Turned right 1
            if enc_direction == "R":
                encoder_rorated_up()

    else:  # enc_state == "11"
        if new_state == "01":  # Turned left 1
            enc_direction = "L"
        elif new_state == "10":  # Turned right 1
            enc_direction = "R"
        elif new_state == "00":  # Skipped an intermediate 01 or 10 state, but if we know direction then a turn is complete
            if enc_direction == "L":
                encoder_rorated_down()
            elif enc_direction == "R":
                encoder_rorated_up()

    enc_state = new_state
    return


def encoder_rorated_up():
    global screen, close_value, open_value, screen_option, serial_number, hw_revision

    if screen == SCREEN_MENU:
        screen_option += 1

    elif screen == SCREEN_CAL_CLOSE:
        close_value += 10
        if close_value > 2000:
            close_value = 2000
        send_command("C", close_value)

    elif screen == SCREEN_CAL_OPEN:
        open_value += 10
        if open_value > 2000:
            open_value = 2000
        send_command("O", open_value)

    elif screen == SCREEN_VERIFY:
        screen_option += 1

    elif screen == SCREEN_SN:
        serial_number += 1

    elif screen == SCREEN_HW_REVISION:
        if hw_revision < 20:
            hw_revision += 1

    elif screen == SCREEN_CONFIRM:
        screen_option += 1

    draw_screen()
    return


def encoder_rorated_down():
    global screen, close_value, open_value, screen_option, serial_number, hw_revision

    if screen == SCREEN_MENU:
        screen_option -= 1

    if screen == SCREEN_CAL_CLOSE:
        close_value -= 10
        if close_value < 600:
            close_value = 600
        send_command("C", close_value)

    elif screen == SCREEN_CAL_OPEN:
        open_value -= 10
        if open_value < close_value:
            open_value = close_value
        send_command("O", open_value)

    elif screen == SCREEN_VERIFY:
        screen_option -= 1

    elif screen == SCREEN_SN:
        if serial_number > 0:
            serial_number -= 1

    elif screen == SCREEN_HW_REVISION:
        if hw_revision > 0:
            hw_revision -= 1

    elif screen == SCREEN_CONFIRM:
        screen_option -= 1

    draw_screen()
    return


def button_pushed(channel):
    global screen, close_value, open_value, serial_number, hw_revision, screen_option

    if screen == SCREEN_MENU:
        option = screen_option % 2
        if option == 0:  # calibration
            screen = SCREEN_CAL_CLOSE
            close_value = 1000
			open_value = 0

        elif option == 1:  # flash firmware
            flash_firmware()
            screen = SCREEN_MENU
            screen_option = 0

    elif screen == SCREEN_CAL_CLOSE:
        screen = SCREEN_CAL_OPEN
		if open_value == 0:
			open_value = close_value + 500

    elif screen == SCREEN_CAL_OPEN:
        screen = SCREEN_VERIFY
        screen_option = 0

    elif screen == SCREEN_VERIFY:
        option = screen_option % 4
        if option == 0: # close
            send_command("C", close_value)

        elif option == 1: # open
            send_command("O", open_value)

        elif option == 2: # confirm
			send_command("C", close_value)
            screen = SCREEN_SN

        elif option == 3: # retry
            screen = SCREEN_CAL_CLOSE

    elif screen == SCREEN_SN:
        send_command("N", serial_number)
        screen = SCREEN_HW_REVISION

    elif screen == SCREEN_HW_REVISION:
        send_command("H", hw_revision)
        screen = SCREEN_CONFIRM
        screen_option = 0

    elif screen == SCREEN_CONFIRM:
        option = screen_option % 2
        if option == 0:  # finish calibration
            send_command("E", 0)
            serial_number += 1  # advance serial number for next Floower
            screen = SCREEN_DISCONNECT

        elif option == 1:  # retry
            screen = SCREEN_CAL_CLOSE

    draw_screen()
    return


def draw_screen():
    global screen, close_value, open_value, serial_number, hw_revision, screen_option

    if screen == SCREEN_CONNECT:
        lcd.clear()
        lcd.message("Pripoj Floower")

    elif screen == SCREEN_MENU:
        lcd.clear()
        lcd.message(" Kalibrace")
        lcd.set_cursor(0, 1)
        lcd.message(" Nahrat Firmware")
        option = screen_option % 2
        if option == 0:
            lcd.set_cursor(0, 0)
            lcd.message(chr(126))
        elif option == 1:
            lcd.set_cursor(0, 1)
            lcd.message(chr(126))

    elif screen == SCREEN_CAL_CLOSE:
        lcd.clear()
        lcd.message("Zavreno")
        lcd.set_cursor(0, 1)
        lcd.message(str(close_value))
        lcd.set_cursor(15, 1)
        lcd.message(chr(126))

    elif screen == SCREEN_CAL_OPEN:
        lcd.clear()
        lcd.message("Otevreno")
        lcd.set_cursor(0, 1)
        lcd.message(str(open_value))
        lcd.set_cursor(15, 1)
        lcd.message(chr(126))

    elif screen == SCREEN_VERIFY:
        lcd.clear()
        lcd.message(" Zavrit       Ok")
        lcd.set_cursor(0, 1)
        lcd.message(" Otevrit   Znovu")
        option = screen_option % 4
        if option == 0:
            lcd.set_cursor(0, 0)
            lcd.message(chr(126))
        elif option == 1:
            lcd.set_cursor(0, 1)
            lcd.message(chr(126))
        elif option == 2:
            lcd.set_cursor(13, 0)
            lcd.message(chr(126))
        elif option == 3:
            lcd.set_cursor(10, 1)
            lcd.message(chr(126))

    elif screen == SCREEN_SN:
        lcd.clear()
        lcd.message("Seriove Cislo")
        lcd.set_cursor(0, 1)
        lcd.message(str(serial_number))
        lcd.set_cursor(15, 1)
        lcd.message(chr(126))

    elif screen == SCREEN_HW_REVISION:
        lcd.clear()
        lcd.message("HW Revize")
        lcd.set_cursor(0, 1)
        lcd.message(str(hw_revision))
        lcd.set_cursor(15, 1)
        lcd.message(chr(126))

    elif screen == SCREEN_CONFIRM:
        lcd.clear()
        lcd.message(" Zapsat")
        lcd.set_cursor(0, 1)
        lcd.message(" Znovu")
        option = screen_option % 2
        if option == 0:
            lcd.set_cursor(0, 0)
            lcd.message(chr(126))
        elif option == 1:
            lcd.set_cursor(0, 1)
            lcd.message(chr(126))

    elif screen == SCREEN_DISCONNECT:
        lcd.clear()
        lcd.message("Odpoj Floower")

    return


def send_command(command, value):
    global serial_connection, connected_device

    if serial_connection is None or serial_connection.is_open == False:
        print("Not connected, cannot send command")
        return

    try:
        command = command + "{:d}".format(value)
        print(command)
        command += "\n"
        serial_connection.write(command.encode())
        return

    except serial.serialutil.SerialException:
        serial_connection.close()
        serial_connection = None
        connected_device = None
        print("Serial COM error - disconnecting,")
        reset()


def flash_firmware():
    global serial_connection, connected_device

    lcd.clear()
    lcd.message("Nahravam ...\n5%")

    esptool_write_factory_reset()

    lcd.clear()
    lcd.message("Nahravam ...\n45%")
    sleep(1)
    lcd.clear()
    lcd.message("Nahravam ...\n50%")

    esptool_write_flash_firmware()

    lcd.clear()
    lcd.message("Hotovo")
    sleep(1)

    serial_connection.close()
    serial_connection = serial.Serial(connected_device, 115200, timeout=1)


def esptool_write_flash_firmware():
    global connected_device

    command = get_esptool_base_comman(connected_device) + " 0x10000 bin/floower-esp32.ino.bin 0x8000 bin/floower-esp32.ino.partitions.bin"

    print("Flashing Floower Firmware");
    print(command);
    os.system(command)


def esptool_write_factory_reset():
    global connected_device

    command = get_esptool_base_comman(connected_device) + " 0x10000 bin/floower-esp32-factoryreset.ino.bin 0x8000 bin/floower-esp32-factoryreset.ino.partitions.bin"

    print("Performing Floower Factory Reset");
    print(command);
    os.system(command)


def get_esptool_base_comman(port):
    return "esptool.py --port " + port + " --chip esp32 -b 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 bin/boot_app0.bin 0x1000 bin/bootloader_dio_80m.bin"


def discover_and_connect_serial():
    global serial_connection, connected_device

    com = "";
    pts = prtlst.comports()

    for pt in pts:
        if 'CP2102' in pt[1] and 'USB' in pt[1]: # search for CP2102 and USB string
            com = pt[0]
            break

    if com:
        print("Connecting to", com)
        connected_device = com
        serial_connection = serial.Serial(com, 115200, timeout=1)
        return True

    return False


def check_serial_device():
    global serial_connection, connected_device

    connected = False
    if connected_device is not None:
        pts = prtlst.comports()
        for pt in pts:
            if connected_device == pt[0]:
                connected = True
                break

        if connected == False: # disconnected
            print("Disconnected from", connected_device)
            serial_connection.close()
            serial_connection = None
            connected_device = None
            reset()

    return


def reset():
    global screen, close_value, open_value, screen_option
    screen = SCREEN_CONNECT
    close_value = 1000
    open_value = 1000
    screen_option = 0
    draw_screen()
    return


def main():
    global screen, screen_option

    try:
        initGPIO()
    except KeyboardInterrupt:
        GPIO.cleanup()

    print("Floower Planter Tool v%s" % (VERSION))
    lcd.clear()
    lcd.message("Floower Planter\nv%s" % (VERSION))
    sleep(5)

    reset()

    while True:
        # try to discover and connect to device
        if serial_connection is None or serial_connection.is_open == False:
            if discover_and_connect_serial() == True:
                lcd.clear()
                lcd.message("Pripojeno")
                sleep(1)
                screen = SCREEN_MENU
                screen_option = 0
                draw_screen()

        else:
            check_serial_device()

        sleep(0.5)

if __name__ == '__main__':
    main()