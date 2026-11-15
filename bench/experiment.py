"""Emit a byte-preserving candidate header selected by experiment name.

Compile with the output directory before the repository in the include search path.
The source header stays unchanged; use fixtures and both platform A/A bands before retaining.
"""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> None:
    text = (Path(__file__).resolve().parents[1] / "sortable.hh").read_text()
    if sys.argv[1] in ("encode-switch", "encode-long-switch"):
        old = "\t\tfor (int i = 0; i < len; ++i) out[n++] = static_cast<char>(tail >> (56 - 8 * i));"
        lines = ["\t\tswitch (len) {"]
        for n in range(8, 0, -1):
            lines.append(
                f"\t\t\tcase {n}: out[n + {n - 1}] = static_cast<char>(tail >> {64 - 8 * n});"
                + (" [[fallthrough]];" if n > 1 else " break;")
            )
        lines += ["\t\t}", "\t\tn += len;"]
        new = "\n".join(lines)
        if sys.argv[1] == "encode-long-switch":
            new = (
                "\t\tif (len < 6) {\n\t"
                + old
                + "\n\t\t} else {\n"
                + "\n".join("\t" + line for line in lines)
                + "\n\t\t}"
            )
    elif sys.argv[1] == "decode-switch":
        old = "\tfor (std::size_t k = 0; k < len; ++k) tail |= static_cast<std::uint64_t>(at(i + k)) << (56 - 8 * k);"
        lines = ["\tswitch (len) {"]
        for n in range(8, 0, -1):
            lines.append(
                f"\t\tcase {n}: tail |= static_cast<std::uint64_t>(at(i + {n - 1})) << {64 - 8 * n}; [[fallthrough]];"
            )
        lines += ["\t\tcase 0: break;", "\t}"]
        new = "\n".join(lines)
    elif sys.argv[1] == "double-normal-fast":
        old = "\t\tint e = exponent;"
        new = (
            "\t\tif (exponent >= -1022 && exponent <= 1023 && (significand & 0x7FF) == 0) {\n"
            "\t\t\treturn std::bit_cast<double>(sign | (static_cast<std::uint64_t>(exponent + 1023) << 52) |\n"
            "\t\t\t\t((significand >> 11) & ((std::uint64_t{1} << 52) - 1)));\n"
            "\t\t}\n" + old
        )
    elif sys.argv[1] in ("decode-fraction", "decode-fraction-narrow"):
        old = (
            "\t// Fraction bits: the position of the lowest set bit of f.\n"
            "\tconst int k = frac == 0 ? 0 : 64 - std::countr_zero(frac);\n"
            "\tbool is_double = false;\n"
            "\tif (e >= min_exp && e <= 1023) is_double = e >= -1022 ? k <= 52 : k <= e - min_exp;\n"
            "\tbool is_integer = false;\n"
            "\tif (e >= 0 && k <= e) is_integer = neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 63;"
        )
        if sys.argv[1] == "decode-fraction":
            new = (
                "\tbool is_double = false;\n"
                "\tif (e >= -1022 && e <= 1023) is_double = (frac & 0xFFF) == 0;\n"
                "\telse if (e >= min_exp && e < -1022) {\n"
                "\t\tconst int k = frac == 0 ? 0 : 64 - std::countr_zero(frac);\n"
                "\t\tis_double = k <= e - min_exp;\n"
                "\t}\n"
                "\tbool is_integer = false;\n"
                "\tif (!is_double && e >= 0) {\n"
                "\t\tconst int k = frac == 0 ? 0 : 64 - std::countr_zero(frac);\n"
                "\t\tif (k <= e) is_integer = neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 63;\n"
                "\t}"
            )
        else:
            new = (
                "\tbool is_double = false;\n"
                "\tint k = 0;\n"
                "\tif (e >= -1022 && e <= 52 && (frac & 0xFFF) == 0) {\n"
                "\t\tis_double = true;\n"
                "\t} else {\n"
                "\t\tk = frac == 0 ? 0 : 64 - std::countr_zero(frac);\n"
                "\t\tif (e >= min_exp && e <= 1023) is_double = e >= -1022 ? k <= 52 : k <= e - min_exp;\n"
                "\t}\n"
                "\tbool is_integer = false;\n"
                "\tif (!is_double && e >= 0 && k <= e)\n"
                "\t\tis_integer = neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 63;"
            )
    else:
        raise ValueError("unknown experiment")
    if text.count(old) != 1:
        raise ValueError("baseline header does not match")
    sys.stdout.write(text.replace(old, new))


if __name__ == "__main__":
    main()
