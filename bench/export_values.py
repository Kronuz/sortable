"""Export the design's actual value sets: export_values.py SIZES_PY ASSETS FILES GIT [MAX_FILES].

SIZES_PY is Xapiand design/sortable-encoding:docs/design/sortable-encoding/sizes.py.
Output is set TAB i|u|d TAB value, consumed by sortable_speed.
"""

from __future__ import annotations

import importlib.util
import sys


def main() -> None:
    path, assets, files, git = sys.argv[1:5]
    spec = importlib.util.spec_from_file_location("sizes", path)
    assert spec and spec.loader
    sizes = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sizes)
    sets = {
        **sizes.ndjson_sets(assets),
        **sizes.file_sets(files, int(sys.argv[5]) if len(sys.argv) > 5 else 200000),
        **sizes.git_sets(git),
    }
    for name, (_, values) in sets.items():
        for value in values:
            kind = "d" if isinstance(value, float) else "u" if value >= 1 << 63 else "i"
            print(name, kind, repr(value), sep="\t")


if __name__ == "__main__":
    main()
