"""Summarize whole-run medians: summarize.py RUN.tsv [RUN.tsv ...].

The interval spans each run's sample median, rather than pooling individual timings.
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path
from statistics import median


def main() -> None:
    runs: list[dict[tuple[str, str], list[dict[str, str]]]] = []
    for filename in sys.argv[1:]:
        rows: dict[tuple[str, str], list[dict[str, str]]] = {}
        with Path(filename).open() as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                rows.setdefault((row["set"], row["type"]), []).append(row)
        runs.append(rows)
    if not runs or any(run.keys() != runs[0].keys() for run in runs):
        raise ValueError("input runs must cover the same distributions")
    print(
        "| Set | Type | Encode ns/value, median [range] | Decode ns/value, median [range] | B/value |"
    )
    print("|---|---|---:|---:|---:|")
    for key, sample_rows in runs[0].items():
        cells: list[str] = []
        for field in ("encode_ns", "decode_ns"):
            per_run = [median(float(row[field]) for row in run[key]) for run in runs]
            cells.append(
                f"{median(per_run):.3f} [{min(per_run):.3f}, {max(per_run):.3f}]"
            )
        print(
            f"| {key[0]} | {key[1]} | {' | '.join(cells)} | {sample_rows[0]['bytes']} |"
        )


if __name__ == "__main__":
    main()
