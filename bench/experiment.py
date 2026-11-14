"""Emit a byte-preserving candidate header: experiment.py encode-switch|decode-switch.

Compile with the output directory before the repository in the include search path.
The source header stays unchanged; use fixtures and both platform A/A bands before retaining.
"""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> None:
    text = (Path(__file__).resolve().parents[1] / "sortable.hh").read_text()
    if sys.argv[1] == "encode-switch":
        old = "\t\tfor (int i = 0; i < len; ++i) out[n++] = static_cast<char>(tail >> (56 - 8 * i));"
        lines = ["\t\tswitch (len) {"]
        for n in range(8, 0, -1):
            lines.append(
                f"\t\t\tcase {n}: out[n + {n - 1}] = static_cast<char>(tail >> {64 - 8 * n});"
                + (" [[fallthrough]];" if n > 1 else " break;")
            )
        lines += ["\t\t}", "\t\tn += len;"]
        new = "\n".join(lines)
    elif sys.argv[1] == "decode-switch":
        old = "\tfor (std::size_t k = 0; k < len; ++k) tail |= static_cast<std::uint64_t>(at(i + k)) << (56 - 8 * k);"
        lines = ["\tswitch (len) {"]
        for n in range(8, 0, -1):
            lines.append(
                f"\t\tcase {n}: tail |= static_cast<std::uint64_t>(at(i + {n - 1})) << {64 - 8 * n}; [[fallthrough]];"
            )
        lines += ["\t\tcase 0: break;", "\t}"]
        new = "\n".join(lines)
    else:
        raise ValueError("unknown experiment")
    if text.count(old) != 1:
        raise ValueError("baseline header does not match")
    sys.stdout.write(text.replace(old, new))


if __name__ == "__main__":
    main()
