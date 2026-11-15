"""Summarize three interleaved runs of each decoder arm.

Usage: summarize_decode.py PREFIX [linux], where files are PREFIX-ARM-RUN.tsv.
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path
from statistics import median


def read_medians(path: Path) -> dict[tuple[str, str], float]:
    samples: dict[tuple[str, str], list[float]] = {}
    with path.open() as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            if row["sample"] == "check":
                continue
            key = (row["set"], row["type"])
            samples.setdefault(key, []).append(float(row["decode_ns"]))
    return {key: median(values) for key, values in samples.items()}


def main() -> None:
    prefix = Path(sys.argv[1])
    linux = len(sys.argv) > 2 and sys.argv[2] == "linux"
    arms = (
        ("string", "safety", "trusted")
        if linux
        else ("old", "string", "safety", "trusted")
    )
    runs = {
        arm: [read_medians(Path(f"{prefix}-{arm}-{run}.tsv")) for run in (1, 2, 3)]
        for arm in arms
    }
    keys = set(runs[arms[0]][0])
    if any(set(run) != keys for arm in arms for run in runs[arm]):
        raise ValueError("decoder arms have different profiles")
    if linux:
        print(
            "| Set | Type | Strict ns [A/A] | Safety ns [A/A] | Trusted ns [A/A] | Trusted vs strict |"
        )
        print("|---|---|---:|---:|---:|---:|")
    else:
        print(
            "| Set | Type | Old ns [A/A] | Strict ns [A/A] | Safety ns [A/A] | Trusted ns [A/A] | Trusted vs old |"
        )
        print("|---|---|---:|---:|---:|---:|---:|")
    for key in runs[arms[0]][0]:
        cells = []
        for arm in arms:
            values = [run[key] for run in runs[arm]]
            cells.append(f"{median(values):.2f} [{min(values):.2f}, {max(values):.2f}]")
        base = median(run[key] for run in runs[arms[0]])
        trusted = median(run[key] for run in runs["trusted"])
        print(
            f"| {key[0]} | {key[1]} | {' | '.join(cells)} | {(trusted / base - 1) * 100:+.1f}% |"
        )


if __name__ == "__main__":
    main()
