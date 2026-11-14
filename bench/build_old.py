"""Build the public-API black-box bench: build_old.py XAPIAND_BUILD OUTPUT [old|new|string].

Reuses generated compiler flags and the existing opaque codec object. Run under quiet light.
The optional new mode uses identical flags for a fair native-codec comparison.
No codec implementation source is opened by this script.
"""

from __future__ import annotations

import shlex
import subprocess
import sys
from pathlib import Path


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    flags = build / "CMakeFiles" / "XAPIAND_OBJ.dir" / "flags.make"
    options: list[str] = []
    for line in flags.read_text().splitlines():
        if line.startswith(("CXX_DEFINES =", "CXX_INCLUDES =", "CXX_FLAGS =")):
            options.extend(shlex.split(line.split("=", 1)[1]))
    mode = sys.argv[3] if len(sys.argv) > 3 else "old"
    mode_flags = {
        "old": ["-DSORTABLE_OLD_CODEC"],
        "new": [],
        "string": ["-DSORTABLE_STRING_CODEC"],
    }[mode]
    root = Path(__file__).resolve().parents[1]
    subprocess.run(
        [
            "/usr/bin/c++",
            *options,
            *mode_flags,
            "-I",
            str(root),
            str(root / "bench" / "speed.cc"),
            str(
                build
                / "CMakeFiles"
                / "XAPIAND_OBJ.dir"
                / "src"
                / "sortable_serialise.cc.o"
            ),
            str(build / "_deps" / "cartesian-build" / "libcartesian.a"),
            "-o",
            sys.argv[2],
        ],
        check=True,
    )


if __name__ == "__main__":
    main()
