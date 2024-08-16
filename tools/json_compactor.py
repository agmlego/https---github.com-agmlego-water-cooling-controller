import json
from glob import glob
from os.path import basename, join, splitext

import arrow
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def import_data():
    data = []
    for pth in glob(join("logs", "*.json")):
        with open(pth) as j_file:
            data.append(json.load(j_file))
            data[-1]["updated"] = arrow.get(
                splitext(basename(pth))[0], "YYYY_MM_DD_HH_mm_ss_SSSZ"
            )
    return pd.json_normalize(data)


if __name__ == "__main__":
    samples = import_data()
    samples.to_csv(join("logs", "compacted_data.csv"))

    fig, ax = plt.subplots()
    ax.scatter(
        samples["reservoir.temperature"],
        samples["reservoir.level_sense"],
        label="level",
    )
    ax.scatter(
        samples["reservoir.temperature"], samples["reservoir.level_ref"], label="ref"
    )
    ax.set_xlabel("Reservoir Temperature (°C)")
    ax.set_ylabel("ADC Reading")
    ax.set_title("eTape Calibration Curve")
    ax.legend()
    plt.show()
