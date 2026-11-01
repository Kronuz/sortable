> Status: CURRENT (2026-11-01) ledger for step 1 of the sortable-codec work: the `sortable` library.

# sortable (library)

- Design: the sortable-codec design, revision 2 (D1-D7, T1-T6, T11), which is not in this repository; the bytes are fixed by `tools/model.py`.
- Repository: `Kronuz/sortable`, branch `main`.
- State: published; CI runs on every push and nightly (Linux GCC, Clang and arm64, macOS arm64 and x86-64, FreeBSD, ASan+UBSan, TSan) and passes on `main`.

## Done

1. `sortable.hh` (D2-D5, D7), constexpr-evaluable; tests, fixtures from `tools/model.py` (113,491 vectors, 39 reject witnesses), README, AGENTS.md, `docs/mechanisms/sortable.md`, CI workflow.
2. T1/T2: `sortable_property` with 10^7 random values per type plus 10^7 cross-type pairs and the boundary sets: 50.9M values, 0 failures. T6: all vectors match. T4 exhaustive: 83.9M short inputs, 0 failures.
3. T5 with R1: `test/size.cc` includes sub-1/16 s sets (7.93 and 7.94 B against 7.99 B) and the deliberate N1 cost (9.93 B against 7.98 B, slack 2.5 B). The old codec's means are constants in `test/size.cc`, from the design's size table.
4. C4 and the D2/D3 mutants: `tools/mutants.py`, 38 mutants, 0 unexpected. Two survive as equivalent (D5.4's upper bound and D5.8 alone, both implied by D5.9); each dies with D5.9 off (D5.8) or is bounded by D5.9 (D5.4). D5.6 alone dies to UBSan's shift check.
5. Sanitizers in separate build dirs: ASan+UBSan and TSan, 4 of 4 tests pass.
6. Fuzz: `fuzz/decode_fuzz.cc`, 4 workers x 10 minutes, 279M runs, 0 findings.
7. C1: the 20 call forms compile and the 7 others do not, as `static_assert`s in `test/test.cc`.

## Next

1. T3: `-DSORTABLE_EXHAUSTIVE=ON`, run `sortable_exhaustive_float` once (minutes; not in CI).
2. One CPU-hour of fuzzing at the gate's commit; the security reviewer's decoder review and a code review.
3. Optionally add the sub-1/16 s sets to the design's size table, so T5 against the old codec guards the window.

## Blocked on

Nothing.
