import sys
import time
from argparse import ArgumentParser

from bluepy import btle
from colr import color as colr


# BLE IoT Sensor Demo
# Author: Gary Stafford
# Reference: https://elinux.org/RPi_Bluetooth_LE
# sudo python3 -m pip install --user bluepy colr


def byte_array_to_int(value):
    # If the value is not recognized as GATT and converted properly, then it is
    # left as a byte array of four int values (hex) which need to be reversed
    # value = bytearray.fromhex(value)
    print(f"{sys._getframe().f_code.co_name}: {value}")
    value = bytearray(value)
    value.reverse()
    value = int.from_bytes(value, byteorder="big")
    return value


def split_color_str_to_array(value):
    # print(f"{sys._getframe().f_code.co_name}: {value}")

    # remove extra bit on end (e.g. b'534,300,234,983\x00')
    value = value[0:-1]

    # split r, g, b, a values into array
    values = value.split(",")

    # convert from 16-bit ints (2^16 or 0-65535)
    # to 8-bit ints (2^8 or 0-255)
    values[:] = [int(v) % 256 for v in values]

    return values


def byte_array_to_char(value):
    # print(f"{sys._getframe().f_code.co_name}: {value}")

    return value.decode("utf-8")


def decimal_exponent_two(value):
    return value / 100


def decimal_exponent_one(value):
    return value / 10


def pascals_to_kilopascals(value):
    # 1 Kilopascal (kPa) is equal to 1000 pascals (Pa)
    # To convert kPa to pascal, multiply the kPa value by 1000
    return value / 1000


def celsius_to_fahrenheit(value):
    return (value * 1.8) + 32


def read_color(service):
    color_char = service.getCharacteristics("936b6a25-e503-4f7c-9349-bcc76c22b8c3")[0]
    color = color_char.read()
    color = byte_array_to_char(color)
    print(f"16-bit Color values (r,g,b,a): {color}")
    color = split_color_str_to_array(color)
    print(f" 8-bit Color values (r,g,b,a): {color[0]},{color[1]},{color[2]},{color[3]}")
    print("Color Swatch")
    print(colr('\t\t', fore=(127, 127, 127), back=(color[0], color[1], color[2])))


def read_pressure(service):
    pressure_char = service.getCharacteristics("2A6D")[0]
    pressure = pressure_char.read()
    pressure = byte_array_to_int(pressure)
    pressure = decimal_exponent_one(pressure)
    pressure = pascals_to_kilopascals(pressure)
    print(f"Barometric Pressure: {round(pressure, 2)} kPa")


def read_humidity(service):
    humidity_char = service.getCharacteristics("2A6F")[0]
    humidity = humidity_char.read()
    humidity = byte_array_to_int(humidity)
    humidity = decimal_exponent_two(humidity)
    print(f"Humidity: {round(humidity, 2)}%")


def read_temperature(service):
    temperature_char = service.getCharacteristics("2A6E")[0]
    temperature = temperature_char.read()
    temperature = byte_array_to_int(temperature)
    temperature = decimal_exponent_two(temperature)
    temperature = celsius_to_fahrenheit(temperature)
    print(f"Temperature: {round(temperature, 2)}°F")


def get_args():
    arg_parser = ArgumentParser(description="BLE IoT Sensor Demo")
    arg_parser.add_argument('mac_address', help="MAC address of device to connect")
    args = arg_parser.parse_args()
    return args


def main():
    # get args
    args = get_args()

    print("Connecting...")
    nano_sense = btle.Peripheral(args.mac_address)

    print("Discovering Services...")
    _ = nano_sense.services
    environmental_sensing_service = nano_sense.getServiceByUUID("181A")

    print("Discovering Characteristics...")
    _ = environmental_sensing_service.getCharacteristics()

    while True:
        print("\n")
        read_temperature(environmental_sensing_service)
        read_humidity(environmental_sensing_service)
        read_pressure(environmental_sensing_service)
        read_color(environmental_sensing_service)

        time.sleep(2)


if __name__ == "__main__":
    main()
