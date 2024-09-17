from typing import Union

import bitstring
import i2cdriver

CAL_RANGE = [0.3, 0.7]
OUTPUT_BITS = 24
PRESSURE_MAX = 2
PRESSURE_MIN = -2
TEMPERATURE_MAX = 150
TEMPERATURE_MIN = -50
SENSOR_ADDRESS = 0x28
COM_PORT = "COM18"

OUTPUT_MIN = CAL_RANGE[0] * 2**OUTPUT_BITS
OUTPUT_MAX = CAL_RANGE[1] * 2**OUTPUT_BITS

Numeric = Union[int, float, complex]
fields = (
    "power",
    "busy",
    "memory_error",
    "math_saturation",
    "raw_pressure",
    "raw_temp",
)

i2c = i2cdriver.I2CDriver(COM_PORT)
assert SENSOR_ADDRESS in i2c.scan(silent=True)


def get_pressure(raw_pressure: Numeric) -> float:
    return ((raw_pressure - OUTPUT_MIN) * (PRESSURE_MAX - PRESSURE_MIN)) / (
        OUTPUT_MAX - OUTPUT_MIN
    ) + PRESSURE_MIN


def get_temp(raw_temp: Numeric) -> float:
    return (raw_temp * (TEMPERATURE_MAX - TEMPERATURE_MIN)) / (
        2**OUTPUT_BITS - 1
    ) + TEMPERATURE_MIN


def parse_response(data: bytes) -> dict[str, Union[bool, Numeric]]:
    bits = bitstring.Bits(data)
    ret = dict(
        zip(
            fields,
            bits.unpack(
                "pad:1,bool,bool,pad:2,bool,pad:1,bool,"  # status byte
                "uint:24,"  # pressure field
                "uint:24"  # temperature field
            ),
        )
    )
    ret["raw_data"] = data
    return ret


def request_measurement():
    i2c.start(SENSOR_ADDRESS, 0)
    i2c.write([0xAA, 0x00, 0x00])
    i2c.stop()


def get_measurement_data() -> bytes:
    i2c.start(SENSOR_ADDRESS, 1)
    data = i2c.read(7)
    i2c.stop()
    return data


def trigger_measurement_cycle() -> dict[str, Union[bool, Numeric]]:
    request_measurement()
    data = get_measurement_data()
    ret = parse_response(data)
    while ret["busy"]:
        data = get_measurement_data()
        ret = parse_response(data)

    ret["pressure"] = get_pressure(ret["raw_pressure"])
    ret["temp"] = get_temp(ret["raw_temp"])
    return ret


if __name__ == "__main__":
    from pprint import pprint

    pprint(trigger_measurement_cycle())
