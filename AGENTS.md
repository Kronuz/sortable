# AGENTS.md

Working notes for agents modifying this repository. For usage read `README.md`; for the
format read `docs/mechanisms/sortable.md`.

## Repo map

```
sortable.hh              The whole library: encode, decode, number, max_finite, min_finite.
tools/model.py           The executable reference model: the format's bytes are its output.
tools/gen_reject.py      Writes test/reject.txt (a witness per D5 rule) from the model.
tools/mutants.py         Drops one rule at a time in a copy of the header; each must fail a test.
tools/make_corpus.py     Fuzz seeds from the fixtures.
tools/cli.cc             sortable_cli encode|decode over stdin lines, for cross-language tests.
tools/check_cli.py       CTest: drives sortable_cli against the model.
tools/check_fixtures.cmake  CTest: regenerates the fixtures and compares.
test/test.cc             Examples, call forms (static_assert), conversions, vectors, reject
                         fixture, all inputs of up to 3 bytes, threads.
test/property.cc         Round trip, order and cross-type equality: exhaustive at boundaries,
                         random over each type (SORTABLE_SEED, SORTABLE_RANDOM).
test/size.cc             Mean bytes on the design's distributions against the old codec's.
test/exhaustive_float.cc All 2^32 floats (opt-in).
test/exact.hh            The codec-independent comparator the tests and the fuzz target share.
test/vectors.txt         "kind value hex" lines written by `model.py vectors`.
test/reject.txt          "rule name hex" lines that must be rejected.
fuzz/decode_fuzz.cc      libFuzzer target; fuzz/standalone.cc replays files without it.
docs/mechanisms/sortable.md  How the encoding works.
docs/status/sortable.md  The build ledger.
```

## Verification and benchmarks

CI (`.github/workflows/ci.yml`) calls the catalog's shared
[`cmake.yml`](https://github.com/Kronuz/cpp-libraries/blob/main/.github/workflows/cmake.yml) on every push and nightly:
CTest on Linux (x86-64 GCC and Clang, arm64), macOS (arm64, x86-64) and FreeBSD, then again under
ASan+UBSan and TSan. The shared method is the catalog's
[PERFORMANCE.md](https://github.com/Kronuz/cpp-libraries/blob/main/docs/PERFORMANCE.md).

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
# ASan+UBSan as CI's linux-asan-ubsan job runs it (Linux, Clang)
CC=clang CXX=clang++ cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS" \
  "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
cmake --build build-asan && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan --output-on-failure
python3 -I tools/mutants.py      # mutation check in build-mutants/, 4 jobs
cmake -S . -B build-x -DSORTABLE_EXHAUSTIVE=ON && cmake --build build-x && build-x/sortable_exhaustive_float
# libFuzzer: a clang that ships it (Linux clang, or Homebrew LLVM on macOS)
CXX=clang++ cmake -S . -B build-fuzz -DSORTABLE_FUZZ=ON && cmake --build build-fuzz
python3 -I tools/make_corpus.py corpus && build-fuzz/sortable_fuzz corpus -max_total_time=600 -timeout=5
```

The reference is `tools/model.py`, an executable Python model of the format: `test/vectors.txt`
(113,491 lines) is its output, and `test/reject.txt` (from `tools/gen_reject.py`) holds a
witness per reject rule. `sortable_fixtures` regenerates both and fails on any difference
(skipped without Python 3). `sortable` checks the header against both files, every input of up
to 3 bytes, and the call forms as `static_assert`s; `sortable_property` checks round trip, order
and cross-type equality against the codec-independent comparator in `test/exact.hh`, at every
boundary and over random values (`SORTABLE_SEED`, `SORTABLE_RANDOM`). `sortable_size` compares
the mean encoded size on 16 seeded distributions with constants for Xapian's
`sortable_serialise`, the codec this one replaces; the program that measured those constants is
not in this repository. `tools/mutants.py` changes one rule at a time in a copy of
`sortable.hh` and builds `test.cc` and `property.cc` with ASan and UBSan; it passes (exit 0,
`0 not as expected`) when a test kills every mutant except the two marked equivalent (D5.4's
bound and D5.8, implied by D5.9), which must survive. `sortable_exhaustive_float` (minutes, not
in CI) covers all 2^32 float bit patterns and fails unless it counts 2^32. `sortable_cli`
(`tools/cli.cc`) runs the codec over stdin lines for the Python port's cross-language test;
the `sortable_cli` test (`tools/check_cli.py`, skipped without Python 3) drives it against the
model, and `docs/mechanisms/sortable.md` gives its protocol. `sortable_standalone_fuzz FILE...` replays
fuzz inputs without libFuzzer. There is no bench target: encoded size is the one performance
claim, and `sortable_size` checks it.

## Invariants

1. The bytes are fixed by `tools/model.py`. A change to `sortable.hh` that alters a byte is a format
   change: it needs a new design, and `test/vectors.txt` must still equal the model's output.
2. `decode(b)` returns a value only if `encode` of that value gives `b` back.
3. The first byte is never `0x00` (a multi-value list's magic in Xapiand) or `0x7F`.
4. No `long double`, no locale, no allocation in `encode(T, char*)` or `decode`.
5. Every reject rule has a witness in `test/reject.txt` and an entry in `tools/mutants.py`.

## Traps

1. The encoding is not prefix-free: concatenate only with framing.
2. -0.0 encodes as 0 and every NaN as `ff`; neither round trips bit for bit.
3. Rules D5.6 and D5.8 are implied by D5.9 (alone they are equivalent mutants); they stay first so the
   decoder bounds its reads.
4. `float` widens to `double` exactly; `long double`, `bool` and the character types do not compile.
