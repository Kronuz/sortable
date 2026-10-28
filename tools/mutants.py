"""Mutation check: each mutant changes one rule of sortable.hh and must make a test fail.

Usage: python3 -I tools/mutants.py [WORKDIR] [JOBS]
Builds the unit and property tests against each mutated copy of the header (ASan and UBSan on, so a read
past the end of the input is a failure too) and prints the first failing check. Exit 1 if a mutant that
should die survives. A mutant marked "equivalent" is a rule another rule already implies (D5.6, D5.8, the
upper exponent bound): it is expected to survive alone and to die together with D5.9.
"""

from __future__ import annotations

import concurrent.futures
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = (ROOT / "sortable.hh").read_text()

# (name, [(find, replace), ...], expected): expected is "dies" or "equivalent".
D9 = ("if (!is_double && !is_integer) return std::nullopt;  // D5.9", "if (false) return std::nullopt;")
MUTANTS: list[tuple[str, list[tuple[str, str]], str]] = [
    # D2/D3, the encoder.
    ("D3 a negative takes the tag of E, not of U", [("u = e + 1;", "u = e;")], "dies"),
    ("D3 the escape field is not complemented", [("field = 0xFFFF - field;", ";")], "dies"),
    ("D3 the tail is f, not 1 - f", [("tail = std::uint64_t{0} - frac;", "tail = frac;")], "dies"),
    ("D3 the tag is not mirrored", [("tag = static_cast<std::uint8_t>(0xFF - tag);", ";")], "dies"),
    ("D2 the last byte is not canonical (zeros kept)", [("const int len = 8 - (std::countr_zero(tail) >> 3);", "const int len = 8;")], "dies"),
    ("D2 the window starts one exponent late", [("u >= lo_exp && u <= hi_exp", "u >= lo_exp + 1 && u <= hi_exp")], "dies"),
    ("D2 the window ends one exponent early", [("u >= lo_exp && u <= hi_exp", "u >= lo_exp && u <= hi_exp - 1")], "dies"),
    ("D2 the low escape field is off by one", [("static_cast<std::uint32_t>(u - min_exp)", "static_cast<std::uint32_t>(u - min_exp + 1)")], "dies"),
    ("D2 the high escape field is off by one", [("static_cast<std::uint32_t>(u - (hi_exp + 1))", "static_cast<std::uint32_t>(u - hi_exp)")], "dies"),
    ("D2 a subnormal's exponent is off by one", [("(63 - lz) + min_exp,", "(63 - lz) + min_exp + 1,")], "dies"),
    ("D4 -0.0 is not zero", [("out[0] = static_cast<char>(tag_zero);  // -0.0 is 0", "out[0] = static_cast<char>(neg ? 0x7E : tag_zero);")], "dies"),
    ("D4 NaN carries its sign", [("out[0] = static_cast<char>(tag_nan);\n\t\t\treturn 1;", "out[0] = static_cast<char>(neg ? 0x7E : tag_nan);\n\t\t\treturn 1;")], "dies"),
    ("D4 -inf has the wrong field", [("out[1] = static_cast<char>(neg ? 0x00 : 0xFF);", "out[1] = static_cast<char>(0xFF);")], "dies"),
    # D5, one reject rule at a time.
    ("D5.1 the empty input is accepted", [("if (size == 0) return std::nullopt;", "if (false) return std::nullopt;")], "dies"),
    ("D5.1 tag 0x00 is accepted", [("at(0) == 0x00 || at(0) == 0x7F", "at(0) == 0x7F")], "dies"),
    ("D5.1 tag 0x7f is accepted", [("at(0) == 0x00 || at(0) == 0x7F", "at(0) == 0x00")], "dies"),
    ("D5.2 zero or NaN with a tail", [("if (size > 1) return std::nullopt;  // D5.2", "if (false) return std::nullopt;")], "dies"),
    ("D5.3 an escape tag without its field", [("if (size < 3) return std::nullopt;  // D5.3", "if (false) return std::nullopt;")], "dies"),
    ("D5.4 an escape exponent that fits a direct tag", [("if (e >= lo_exp) return std::nullopt;  // D5.4: it fits a direct tag", "if (false) return std::nullopt;")], "dies"),
    ("D5.4 an escape exponent beyond 1023 (1024)", [("if (e > (neg ? 1024 : 1023)) return std::nullopt;  // D5.4: beyond a double", "if (false) return std::nullopt;")], "equivalent"),
    ("D5.5 an infinity with a tail", [("if (size > 3) return std::nullopt;  // D5.5", "if (false) return std::nullopt;")], "dies"),
    ("D5.6 a tail of more than 8 bytes", [("if (len > 8) return std::nullopt;", "if (false) return std::nullopt;")], "dies"),
    ("D5.6 + D5.9", [("if (len > 8) return std::nullopt;", "if (false) return std::nullopt;"), D9], "dies"),
    ("D5.7 a tail ending in 0", [("if (len != 0 && at(size - 1) == 0) return std::nullopt;", "if (false) return std::nullopt;")], "dies"),
    ("D5.8 a 64-bit fraction", [("if (len == 8 && (at(size - 1) & 1) != 0) return std::nullopt;", "if (false) return std::nullopt;")], "equivalent"),
    ("D5.8 + D5.9", [("if (len == 8 && (at(size - 1) & 1) != 0) return std::nullopt;", "if (false) return std::nullopt;"), D9], "dies"),
    ("D5.9 neither a double nor an integer", [D9], "dies"),
    ("D5.9 a double may have 53 fraction bits", [("is_double = e >= -1022 ? k <= 52 :", "is_double = e >= -1022 ? k <= 53 :")], "dies"),
    ("D5.9 a subnormal may have one more bit", [("k <= e - min_exp;", "k <= e - min_exp + 1;")], "dies"),
    ("D5.9 a double may reach 2^1024", [("if (e >= min_exp && e <= 1023)", "if (e >= min_exp && e <= 1024)")], "dies"),
    ("D5.9 an integer may have a fraction bit", [("if (e >= 0 && k <= e)", "if (e >= 0 && k <= e + 1)")], "dies"),
    ("D5.9 a negative integer may pass -2^63", [("neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 63", "e <= 63")], "dies"),
    ("D5.9 an integer may pass 2^64", [("neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 63", "neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 64")], "dies"),
    # The typed conversions.
    ("to_double rounds ties up", [("if (rem > half || (rem == half && (q & 1))) ++q;", "if (rem >= half) ++q;")], "dies"),
    ("to_double truncates", [("if (rem > half || (rem == half && (q & 1))) ++q;", ";")], "dies"),
    ("saturate_int64 wraps", [("if (exponent > 63) return negative ? lo : hi;", "if (exponent > 63) return 0;")], "dies"),
    ("saturate_uint64 keeps negatives", [("if (negative || exponent < 0) return 0;", "if (exponent < 0) return 0;")], "dies"),
    ("to_int64 accepts a fraction", [("if ((significand & ((std::uint64_t{1} << shift) - 1)) != 0) return false;", ";")], "dies"),
]


def run(args: tuple[int, str, list[tuple[str, str]], str, Path]) -> tuple[int, str, str, str]:
    i, name, edits, expected, work = args
    src = HEADER
    for find, repl in edits:
        if src.count(find) != 1:
            return i, name, "ERROR", f"pattern found {src.count(find)} times: {find!r}"
        src = src.replace(find, repl)
    d = work / f"m{i:02d}"
    d.mkdir(parents=True, exist_ok=True)
    (d / "sortable.hh").write_text(src)
    cxx = os.environ.get("CXX", "c++")
    flags = ["-std=c++20", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-DSORTABLE_MUTANT_RUN", f"-I{d}", f"-I{ROOT / 'test'}"]
    outcome = "SURVIVED"
    detail = ""
    for prog, extra, env in (
        ("test", [f'-DSORTABLE_TEST_DIR="{ROOT / "test"}"', "-pthread"], {}),
        ("property", [], {"SORTABLE_RANDOM": "20000"}),
    ):
        exe = d / prog
        cc = subprocess.run([cxx, *flags, *extra, str(ROOT / "test" / f"{'test' if prog == 'test' else 'property'}.cc"), "-o", str(exe)], capture_output=True, text=True)
        if cc.returncode != 0:
            # A mutant the header's own constexpr evaluation rejects is caught at compile time.
            err = next((ln for ln in cc.stderr.splitlines() if "error:" in ln), "compile failed")
            return i, name, "killed", f"compile error: {err.split('error:', 1)[-1].strip()[:120]}"
        r = subprocess.run([str(exe)], capture_output=True, text=True, env={**os.environ, **env})
        if r.returncode != 0:
            first = next((ln for ln in r.stdout.splitlines() if ln.startswith("FAIL")), None) or next(
                (ln for ln in r.stderr.splitlines() if "ERROR" in ln or "runtime error" in ln), "exit %d" % r.returncode
            )
            outcome, detail = "killed", f"{prog}: {first[:150]}"
            break
    return i, name, outcome, detail


def main() -> int:
    work = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build-mutants"
    jobs = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    work.mkdir(parents=True, exist_ok=True)
    bad = 0
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for i, name, outcome, detail in ex.map(run, [(i, n, e, x, work) for i, (n, e, x) in enumerate(MUTANTS)]):
            expected = MUTANTS[i][2]
            ok = (outcome == "killed") == (expected == "dies") and outcome != "ERROR"
            bad += not ok
            print(f"{'ok  ' if ok else 'BAD '}{outcome:8} {name} | {detail}", flush=True)
    print(f"{len(MUTANTS)} mutants, {bad} not as expected")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
