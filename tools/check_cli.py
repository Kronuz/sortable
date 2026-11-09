"""Drives sortable_cli against the reference model, the way the cross-language harness drives it.

Usage: python3 -I tools/check_cli.py SORTABLE_CLI [SEED]
encode: every line of test/vectors.txt, then seeded random integers and binary64 bit patterns.
decode: every vector, every reject witness, every 0-2 byte string, every 3-byte string starting
with an escape tag, then seeded mutations of the vectors; each answer must equal the model's, and
each accepted answer fed back to `encode` must give the input. Malformed lines must exit 2.
Exit 1 on a mismatch.
"""

from __future__ import annotations

import math
import random
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import model  # noqa: E402

ESCAPES = (model.ESC_LOW, model.ESC_HIGH, 0xFF - model.ESC_LOW, 0xFF - model.ESC_HIGH)
MUTATIONS = 200_000
RANDOM_VALUES = 100_000


def bits(d: float) -> str:
    return struct.pack(">d", d).hex()


def value_line(v: int | float) -> str:
    """A value as encode reads it."""
    return f"f {bits(v)}" if isinstance(v, float) else f"i {v}"


def model_decode(b: bytes) -> str:
    """The model's answer in decode's output form."""
    x = model.decode(b)
    if x is None:
        return "reject"
    if isinstance(x, float):
        return f"f {bits(math.nan if math.isnan(x) else x)}"
    if x.denominator == 1 and -(1 << 63) <= x < (1 << 64):
        return f"i {x.numerator}"
    return f"f {bits(float(x))}"


def run(cli: str, mode: str, lines: list[str]) -> list[str]:
    done = subprocess.run(
        [cli, mode],
        input="".join(f"{s}\n" for s in lines),
        capture_output=True,
        text=True,
        check=False,
    )
    if done.returncode != 0:
        sys.exit(f"sortable_cli {mode} exited {done.returncode}: {done.stderr.strip()}")
    out = done.stdout.split("\n")
    if out[-1] != "" or len(out) - 1 != len(lines):
        sys.exit(f"sortable_cli {mode}: {len(out) - 1} lines out for {len(lines)} in")
    return out[:-1]


def compare(what: str, inputs: list[str], got: list[str], want: list[str]) -> int:
    bad = [(i, g, w) for i, g, w in zip(inputs, got, want) if g != w]
    for i, g, w in bad[:5]:
        print(f"{what}: {i!r}: cli {g!r}, model {w!r}")
    print(f"{what}: {len(inputs)} lines, {len(bad)} mismatches")
    return len(bad)


def mutate(rnd: random.Random, seed: bytes) -> bytes:
    m = bytearray(seed)
    for _ in range(rnd.randrange(1, 4)):
        op = rnd.randrange(4)
        if op == 0 and m:
            m[rnd.randrange(len(m))] = rnd.getrandbits(8)
        elif op == 1 and m:
            m[rnd.randrange(len(m))] ^= 1 << rnd.randrange(8)
        elif op == 2 and m:
            del m[rnd.randrange(len(m))]
        else:
            m.insert(rnd.randrange(len(m) + 1), rnd.getrandbits(8))
    return bytes(m)


def main() -> None:
    cli = sys.argv[1]
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 3)
    bad = 0

    # encode: the vectors, then random values against the model.
    values: list[int | float] = []
    want: list[str] = []
    for line in (ROOT / "test" / "vectors.txt").read_text().splitlines():
        kind, text, hexed = line.split()
        values.append(float(text) if kind == "d" else int(text))
        want.append(hexed)
    for _ in range(RANDOM_VALUES):
        if rnd.random() < 0.5:
            v: int | float = rnd.randrange(-(1 << 63), 1 << 64)
        else:
            v = struct.unpack(">d", rnd.getrandbits(64).to_bytes(8, "big"))[0]
        values.append(v)
        want.append(model.encode(v).hex())
    lines = [value_line(v) for v in values]
    bad += compare("encode", lines, run(cli, "encode", lines), want)

    # decode: canonical, rejected, short, escape-tagged and mutated inputs against the model.
    vectors = [bytes.fromhex(h) for h in want]
    inputs = list(vectors)
    for line in (ROOT / "test" / "reject.txt").read_text().splitlines():
        hexed = line.split()[-1]
        inputs.append(b"" if hexed == "-" else bytes.fromhex(hexed))
    inputs.append(b"")
    inputs += [bytes([a]) for a in range(256)]
    inputs += [bytes([a, b]) for a in range(256) for b in range(256)]
    inputs += [bytes([t, a, b]) for t in ESCAPES for a in range(256) for b in range(256)]
    inputs += [mutate(rnd, rnd.choice(vectors)) for _ in range(MUTATIONS)]
    hexes = [b.hex() for b in inputs]
    answers = run(cli, "decode", hexes)
    bad += compare("decode", hexes, answers, [model_decode(b) for b in inputs])

    # Every accepted answer re-encodes to its input.
    accepted = [(h, a) for h, a in zip(hexes, answers) if a != "reject"]
    back = run(cli, "encode", [a for _, a in accepted])
    bad += compare("decode then encode", [a for _, a in accepted], back, [h for h, _ in accepted])

    # Malformed lines are errors, not answers.
    for mode, line in (
        ("encode", "i 18446744073709551616"),
        ("encode", "i -9223372036854775809"),
        ("encode", "i +1"),
        ("encode", "i "),
        ("encode", "f 3ff000000000000"),
        ("encode", "f 3ff00000000000000"),
        ("encode", "f 0x3ff0000000000000"),
        ("encode", "d 1.0"),
        ("decode", "abc"),
        ("decode", "zz"),
        ("decode", "80 "),
    ):
        done = subprocess.run(
            [cli, mode], input=f"{line}\n", capture_output=True, text=True, check=False
        )
        if done.returncode != 2 or done.stdout:
            print(f"malformed {mode} {line!r}: exit {done.returncode}, output {done.stdout!r}")
            bad += 1
    if subprocess.run([cli], capture_output=True, check=False).returncode != 2:
        print("no mode: not exit 2")
        bad += 1

    # The boundaries of the integer range, both directions.
    edges = ["i -9223372036854775808", "i 18446744073709551615", "i 0", "i -0"]
    got = run(cli, "encode", edges)
    want = [
        model.encode(-(1 << 63)).hex(),
        model.encode((1 << 64) - 1).hex(),
        "80",
        "80",
    ]
    bad += compare("integer edges", edges, got, want)

    print("sortable_cli:", "ok" if bad == 0 else f"{bad} failures")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
