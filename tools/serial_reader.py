import json
import time

import arrow
from pySerialTransfer import pySerialTransfer as txfer
from rich.console import Group
from rich.live import Live
from rich.panel import Panel
from rich.progress import (
    BarColumn,
    Progress,
    SpinnerColumn,
    TaskProgressColumn,
    TextColumn,
)

PORT = "COM17"
BAUD = 19200


def get_readings(link: txfer.SerialTransfer):
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

    recSize = 0
    readings["reservoir"]["temperature"] = link.rx_obj(obj_type="f", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["reservoir"]["setpoint"] = link.rx_obj(obj_type="f", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["reservoir"]["level_sense"] = link.rx_obj(obj_type="f", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["reservoir"]["level_ref"] = link.rx_obj(obj_type="f", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["chassis"]["inside_temperature"] = link.rx_obj(
        obj_type="f", start_pos=recSize
    )
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["chassis"]["outside_temperature"] = link.rx_obj(
        obj_type="f", start_pos=recSize
    )
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["chassis"]["humidity"] = link.rx_obj(obj_type="f", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["chassis"]["filter_dp"] = link.rx_obj(obj_type="h", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["h"]
    readings["chassis"]["fans"]["top_tach"] = link.rx_obj(
        obj_type="f", start_pos=recSize
    )
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["chassis"]["fans"]["bottom_tach"] = link.rx_obj(
        obj_type="f", start_pos=recSize
    )
    recSize += txfer.STRUCT_FORMAT_LENGTHS["f"]
    readings["chassis"]["fans"]["pwm"] = link.rx_obj(obj_type="b", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
    readings["compressor"]["running"] = link.rx_obj(obj_type="b", start_pos=recSize) > 0
    recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
    readings["compressor"]["valve"] = link.rx_obj(obj_type="b", start_pos=recSize) > 0
    recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
    readings["compressor"]["compressor_time"] = link.rx_obj(
        obj_type="i", start_pos=recSize
    )
    recSize += txfer.STRUCT_FORMAT_LENGTHS["i"]
    readings["compressor"]["valve_time"] = link.rx_obj(obj_type="i", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["i"]
    readings["pump"]["running"] = link.rx_obj(obj_type="b", start_pos=recSize) > 0
    recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
    readings["pump"]["flow_ok"] = link.rx_obj(obj_type="b", start_pos=recSize) > 0
    recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
    readings["error"]["alert"] = link.rx_obj(obj_type="b", start_pos=recSize) > 0
    recSize += txfer.STRUCT_FORMAT_LENGTHS["b"]
    readings["error"]["code"] = link.rx_obj(obj_type="h", start_pos=recSize)
    recSize += txfer.STRUCT_FORMAT_LENGTHS["h"]

    return readings


if __name__ == "__main__":
    link = txfer.SerialTransfer("COM17", baud=19200)
    link.open()
    time.sleep(2)
    started = False

    res_meters = Progress(
        TextColumn(
            "[progress.description]{task.description} "
            "[{task.fields[low]}{task.fields[unit]}..{task.fields[high]}{task.fields[unit]}] {task.completed}{task.fields[unit]}"
        ),
        BarColumn(),
        auto_refresh=False,
    )
    res_temp_meter = res_meters.add_task("Temp", start=False, unit="°C", low=5, high=30)
    res_lvl_meter = res_meters.add_task(
        "Level", start=False, unit="b", low=0, high=1023, total=1023
    )
    res_ref_meter = res_meters.add_task(
        "R_ref", start=False, unit="b", low=0, high=1023, total=1023
    )

    chassis_meters = Progress(
        TextColumn(
            "[progress.description]{task.description} "
            "[{task.fields[low]}{task.fields[unit]}..{task.fields[high]}{task.fields[unit]}] {task.completed}{task.fields[unit]}"
        ),
        BarColumn(),
        auto_refresh=False,
    )
    chassis_in_temp_meter = chassis_meters.add_task(
        "In Temp", start=False, unit="°C", low=0, high=100
    )
    chassis_humid_meter = chassis_meters.add_task(
        "In RH", start=False, unit="%", low=0, high=80
    )
    chassis_out_temp_meter = chassis_meters.add_task(
        "Out Temp", start=False, unit="°C", low=0, high=100
    )
    chassis_filter_meter = chassis_meters.add_task(
        "Filter ΔP", start=False, unit="b", low=0, high=1023, total=1023
    )
    fan_meters = Progress(
        TextColumn(
            "[progress.description]{task.description} "
            "[{task.fields[low]}{task.fields[unit]}..{task.fields[high]}{task.fields[unit]}] {task.completed}{task.fields[unit]}"
        ),
        BarColumn(),
        auto_refresh=False,
    )
    fan_top_tach = fan_meters.add_task(
        "Top Tach", start=False, unit="RPM", low=0, high=6000, total=6000
    )
    fan_bottom_tach = fan_meters.add_task(
        "Bottom Tach", start=False, unit="RPM", low=0, high=6000, total=6000
    )
    fan_pwm = fan_meters.add_task(
        "PWM", start=False, unit="b", low=0, high=255, total=255
    )

    state_meters = Progress(
        TextColumn("[progress.description]{task.description}"),
        SpinnerColumn(),
        auto_refresh=False,
    )
    state_compressor = state_meters.add_task("Compressor", start=False)
    state_valve = state_meters.add_task("Valve", speed=0, start=False)
    state_pump = state_meters.add_task("Pump", start=False)
    state_flow = state_meters.add_task("Flow", style="point", start=False)

    meter_group = Group(
        Panel(res_meters, title="Reservoir", expand=False),
        Panel(
            Group(chassis_meters, Panel(fan_meters, title="Fans", expand=False)),
            title="Chassis",
            expand=False,
        ),
        Panel(state_meters, title="States", expand=False),
    )
    try:
        with Live(meter_group):

            while True:
                if link.available():
                    readings = get_readings(link)
                    if not started:
                        started = True
                        for task in res_meters.task_ids:
                            res_meters.start_task(task)
                        for task in chassis_meters.task_ids:
                            chassis_meters.start_task(task)
                        for task in fan_meters.task_ids:
                            fan_meters.start_task(task)
                        for task in state_meters.task_ids:
                            state_meters.start_task(task)
                    res_meters.update(
                        res_temp_meter,
                        description=f"Temp (set={readings['reservoir']['setpoint']}°C)",
                        completed=readings["reservoir"]["temperature"],
                        refresh=True,
                    )
                    res_meters.update(
                        res_lvl_meter,
                        completed=int(readings["reservoir"]["level_sense"]),
                        refresh=True,
                    )
                    res_meters.update(
                        res_ref_meter,
                        completed=int(readings["reservoir"]["level_ref"]),
                        refresh=True,
                    )
                    chassis_meters.update(
                        chassis_in_temp_meter,
                        completed=round(readings["chassis"]["inside_temperature"], 1),
                        refresh=True,
                    )
                    chassis_meters.update(
                        chassis_out_temp_meter,
                        completed=readings["chassis"]["outside_temperature"],
                        refresh=True,
                    )
                    chassis_meters.update(
                        chassis_humid_meter,
                        completed=int(readings["chassis"]["humidity"]),
                        refresh=True,
                    )
                    chassis_meters.update(
                        chassis_filter_meter,
                        completed=int(readings["chassis"]["filter_dp"]),
                        refresh=True,
                    )
                    fan_meters.update(
                        fan_top_tach,
                        completed=int(readings["chassis"]["fans"]["top_tach"]),
                        refresh=True,
                    )
                    fan_meters.update(
                        fan_bottom_tach,
                        completed=int(readings["chassis"]["fans"]["bottom_tach"]),
                        refresh=True,
                    )
                    fan_meters.update(
                        fan_pwm,
                        completed=int(readings["chassis"]["fans"]["pwm"]),
                        refresh=True,
                    )
                    state_meters.update(
                        state_compressor,
                        visible=readings["compressor"]["running"],
                        refresh=True,
                    )
                    state_meters.update(
                        state_valve,
                        visible=readings["compressor"]["valve"],
                        refresh=True,
                    )
                    state_meters.update(
                        state_pump,
                        visible=readings["pump"]["running"],
                        refresh=True,
                    )
                    state_meters.update(
                        state_flow,
                        visible=readings["pump"]["flow_ok"],
                        refresh=True,
                    )

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
