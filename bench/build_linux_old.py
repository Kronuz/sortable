"""Package an existing Xapiand public codec build for the Linux comparison.

Usage: build_linux_old.py XAPIAND_BUILD VM_ROOT OUTPUT_DIR
The existing object's dependency file identifies the opaque source closure.
"""

from __future__ import annotations

import shlex
import subprocess
import sys
import tarfile
from pathlib import Path


def dependencies(text: str, build: Path, root: Path) -> set[Path]:
    result: set[Path] = set()
    for word in text.replace("\\\n", " ").split()[1:]:
        path = Path(word)
        if not path.is_absolute():
            path = build / path
        path = path.resolve()
        if path.is_file() and path.is_relative_to(root):
            result.add(path)
    return result


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    root = build.parent
    vm = Path(sys.argv[2])
    output = Path(sys.argv[3])
    output.mkdir(parents=True, exist_ok=True)
    sortable = Path(__file__).resolve().parents[1]
    flags = (build / "CMakeFiles/XAPIAND_OBJ.dir/flags.make").read_text().splitlines()
    options: list[str] = []
    for line in flags:
        if line.startswith(("CXX_DEFINES =", "CXX_INCLUDES =")):
            options.extend(shlex.split(line.split("=", 1)[1]))

    probe_deps = subprocess.run(
        [
            "/usr/bin/c++",
            *options,
            "-DSORTABLE_OLD_CODEC",
            "-I",
            str(sortable),
            "-M",
            str(sortable / "bench/speed.cc"),
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    codec_deps = (
        build / "CMakeFiles/XAPIAND_OBJ.dir/src/sortable_serialise.cc.o.d"
    ).read_text()
    files = dependencies(probe_deps, build, root) | dependencies(
        codec_deps, build, root
    )
    cartesian = build / "_deps/cartesian-src"
    files.update(
        path.resolve()
        for path in cartesian.rglob("*")
        if path.is_file() and ".git" not in path.parts
    )
    with tarfile.open(output / "linux-old-closure.tar.gz", "w:gz") as archive:
        for path in sorted(files):
            archive.add(path, arcname=str(path.relative_to(root)), recursive=False)

    vm_old = vm / "old-src"
    vm_options = [option.replace(str(root), str(vm_old)) for option in options]
    common = [
        "g++",
        "-O3",
        "-g",
        "-DNDEBUG",
        "-std=gnu++20",
        "-ffunction-sections",
        "-fdata-sections",
        *vm_options,
        f"-I{vm}",
        f"-I{vm_old / 'src'}",
    ]
    script = [
        "#!/bin/sh",
        "set -eu",
        shlex.join(
            [
                *common,
                "-c",
                str(vm_old / "src/sortable_serialise.cc"),
                "-o",
                str(vm / "build/old.o"),
            ]
        ),
        shlex.join(
            [
                *common,
                "-DSORTABLE_OLD_CODEC",
                str(vm / "bench/speed.cc"),
                str(vm / "build/old.o"),
                str(vm_old / "build/_deps/cartesian-src/cartesian.cc"),
                "-Wl,--gc-sections",
                "-luuid",
                "-lssl",
                "-lcrypto",
                "-o",
                str(vm / "build/probe-old"),
            ]
        ),
        shlex.join(["file", str(vm / "build/probe-old")]),
    ]
    (output / "linux-old-build.sh").write_text("\n".join(script) + "\n")
    print(f"{len(files)} dependency files packaged")


if __name__ == "__main__":
    main()
