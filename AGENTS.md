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

## Build and test

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Sanitizers go in their own build directories (`build-asan`, `build-tsan`).
Fuzz: `cmake -B build-fuzz -DSORTABLE_FUZZ=ON` with a clang that ships libFuzzer, then
`python3 -I tools/make_corpus.py corpus && build-fuzz/sortable_fuzz corpus -max_total_time=600 -timeout=5`.

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
