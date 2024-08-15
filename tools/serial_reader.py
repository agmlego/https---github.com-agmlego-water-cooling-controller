import json
import time

import arrow
import rich
from pySerialTransfer import pySerialTransfer as txfer

if __name__ == "__main__":
    try:
        link = txfer.SerialTransfer("COM17", baud=19200)
        link.open()
        time.sleep(2)

        readings = {
            "reservoir": {
                "temperature": 0.0,
                "setpoint": 0.0,
                "level_sense": 0.0,
                "level_ref": 0.0,
            },
            "chassis": {
                "inside_temperature": 0.0,
                "outside_temperature": 0.0,
                "humidity": 0.0,
                "filter_dp": 0,
                "fans": {
                    "top_tach": 0.0,
                    "bottom_tach": 0.0,
                    "pwm": 0,
                },
            },
            "compressor": {
                "running": False,
                "valve": False,
                "compressor_time": 0,
                "valve_time": 0,
            },
            "pump": {
                "running": False,
                "flow_ok": False,
            },
            "error": {
                "alert": False,
                "code": 0,
            },
        }

        while True:
            if link.available():
                recSize = 0
                readings["reservoir"]["temperature"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["reservoir"]["setpoint"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["reservoir"]["level_sense"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["reservoir"]["level_ref"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["chassis"]["inside_temperature"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["chassis"]["outside_temperature"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["chassis"]["humidity"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["chassis"]["filter_dp"] = link.rx_obj(
                    obj_type="h", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["h"]
                readings["chassis"]["fans"]["top_tach"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["chassis"]["fans"]["bottom_tach"] = link.rx_obj(
                    obj_type="f", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
                readings["chassis"]["fans"]["pwm"] = link.rx_obj(
                    obj_type="b", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
                readings["compressor"]["running"] = (
                    link.rx_obj(obj_type="b", start_pos=recSize) > 0
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
                readings["compressor"]["valve"] = (
                    link.rx_obj(obj_type="b", start_pos=recSize) > 0
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
                readings["compressor"]["compressor_time"] = link.rx_obj(
                    obj_type="i", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["i"]
                readings["compressor"]["valve_time"] = link.rx_obj(
                    obj_type="i", start_pos=recSize
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["i"]
                readings["pump"]["running"] = (
                    link.rx_obj(obj_type="b", start_pos=recSize) > 0
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
                readings["pump"]["flow_ok"] = (
                    link.rx_obj(obj_type="b", start_pos=recSize) > 0
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
                readings["error"]["alert"] = (
                    link.rx_obj(obj_type="b", start_pos=recSize) > 0
                )
                recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
                readings["error"]["code"] = link.rx_obj(obj_type="h", start_pos=recSize)
                recSize += txfer.STRUCT_FORMAT_LENGTHS["h"]
                rich.inspect(readings, sort=False, title="Readings")
                with open(
                    f'logs/{arrow.get(tzinfo="America/Detroit").format("YYYY_MM_DD_HH_mm_ss_SSSZ")}.json',
                    "w",
                    encoding="utf-8",
                ) as f:
                    json.dump(readings, f)
            elif link.status < 0:
                if link.status == txfer.Status.CRC_ERROR:
                    print("ERROR: CRC_ERROR")
                elif link.status == txfer.Status.PAYLOAD_ERROR:
                    print("ERROR: PAYLOAD_ERROR")
                elif link.status == txfer.Status.STOP_BYTE_ERROR:
                    print("ERROR: STOP_BYTE_ERROR")
                else:
                    print(f"ERROR: {link.status.name}")

    except KeyboardInterrupt:
        link.close()
