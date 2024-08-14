from csv import DictWriter

LEVELS = (0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0)
HEADERS = ("level avg.", "level std.", "ref avg.", "ref std.", "out temp.", "res temp.")

raw_data = open("eTape_Readings.csv").read()

blocks = [block for block in raw_data.split("Reset!") if block]
data = []

for level, block in zip(LEVELS, blocks):
    for row in block.split("\n"):
        if not row:
            continue
        if "!" in row:
            continue
        if "Error" in row:
            continue
        d = {"liters": level}
        d.update(dict(zip(HEADERS, map(float, row.split(",")))))
        data.append(d)

with open("eTape_Processed.csv", "w", newline="", encoding="utf-8") as f:
    csv = DictWriter(f, fieldnames=("liters",) + HEADERS)
    csv.writeheader()
    csv.writerows(data)
