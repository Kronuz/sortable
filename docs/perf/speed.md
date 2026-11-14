# Sortable encode and decode benchmark

C1 is partial. `sortable:04fc571` adds a Release microbenchmark and a real-value exporter;
`sortable.hh`, the model, vectors and reject fixture are unchanged from `sortable:ac13b77`.
No optimization is retained. The old-codec black-box timing, A/A interval and Linux timing
are unmeasured, so this report does not establish D87's speed bar.

The benchmark measures allocation-free `encode(T, char*)` and `decode` followed by the exact
typed conversion, separately, excluding corpus generation and allocation. It warms once and
prints seven independent samples in ns/value and bytes/value. Integers use uniform seeded
8-, 16-, 32- and 64-bit values; doubles use uniform binary64 bit patterns (including specials).
Real integer sets also run as uint64 when nonnegative and as double; the latter conversion may
round integers above 2**53 and must not be used as an exact-integer performance comparison.

| Set | Type | Values | Old codec ns/value | Sortable before ns/value | Sortable after ns/value | Sortable B/value |
|---|---|---:|---|---|---|---:|
| Uniform 8-bit | int64 / uint64 | 10,000 each | Unmeasured | Unmeasured | Unchanged | 1.9390 / 1.9650 |
| Uniform 16-bit | int64 / uint64 | 10,000 each | Unmeasured | Unmeasured | Unchanged | 2.9346 / 2.9630 |
| Uniform 32-bit | int64 / uint64 | 10,000 each | Unmeasured | Unmeasured | Unchanged | 4.9375 / 4.9613 |
| Uniform 64-bit | int64 / uint64 | 10,000 each | Unmeasured | Unmeasured | Unchanged | 8.9402 / 8.9672 |
| Uniform binary64 bits | double | 10,000 | Unmeasured | Unmeasured | Unchanged | 9.8150 |
| accounts._id | int64 / uint64 / double | 1,000 | Unmeasured | Unmeasured | Unchanged | 2.2340 |
| accounts.accountNumber | int64 / uint64 / double | 1,000 | Unmeasured | Unmeasured | Unchanged | 3.7390 |
| accounts.age | int64 / uint64 / double | 1,000 | Unmeasured | Unmeasured | Unchanged | 1.9530 |
| accounts.balance | double | 1,000 | Unmeasured | Unmeasured | Unchanged | 7.8220 |
| cities._id | int64 / uint64 / double | 1,000 | Unmeasured | Unmeasured | Unchanged | 2.2340 |
| cities.population | int64 / uint64 / double | 1,000 | Unmeasured | Unmeasured | Unchanged | 3.1070 |
| cities coordinates as floating | double | 2,000 | Unmeasured | Unmeasured | Unchanged | 7.9425 |
| File sizes | int64 / uint64 / double | 200,000 | Unmeasured | Unmeasured | Unchanged | 2.1920 |
| File mtimes, us | double | 200,000 | Unmeasured | Unmeasured | Unchanged | 6.2704 |
| Git commit times, s | double | 26,977 | Unmeasured | Unmeasured | Unchanged | 4.9854 |

These are untimed encoded-size observations, not performance measurements. The local corpus
has 434,977 original values; file and git sets change with the local tree. Its SHA-256 is
`7b5c20d8250470d05689afb95eadf0bef6a811fa5209ade7de972e8dfedcc012`.
The original design snapshot contains 430,166 values, including 22,166 git commit times;
this export uses the same generator against the current filesystem and repository history.

## Verification and commands

Host: x86_64 macOS, AppleClang 17.0.0.17000604, CMake Release. `file build/sortable_speed`
reports a Mach-O x86_64 executable. The following ran under `quiet light` (10 minutes each):
Release build, all five CTests (including property, fixtures and model/CLI comparisons), and
`sortable_speed build/real-values.tsv 0` (untimed validation of every benchmark distribution).
All passed, and the standalone fuzz driver replayed all 190 existing corpus files with
no failure. Ruff and Pyrefly strict checked the Python exporter. The codec implementation
is byte-for-byte unchanged.

The branch push does not trigger this repository's branch-filtered CI. A manual dispatch
started [CI run 43537630078](https://github.com/Kronuz/sortable/actions/runs/43537630078)
at `sortable:2f9a5ab`; its result was pending when this note was written.

From the sortable checkout, with `ROOT=/Users/kronuz/code/Kronuz`:

```sh
quiet check
quiet light c1-build 10 'Build codec benchmark' -- sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4'
quiet light c1-tests 10 'Codec tests' -- ctest --test-dir build -j4 --output-on-failure
quiet light c1-replay 10 'Replay existing fuzz corpus' -- sh -c 'build/sortable_standalone_fuzz fuzz/corpus/*'
git -C "$ROOT/Xapiand" show design/sortable-encoding:docs/design/sortable-encoding/sizes.py > build/design-sizes.py
quiet light c1-corpus 10 'Export actual design values' -- sh -c 'python3 -I bench/export_values.py build/design-sizes.py "$1/docs/public/assets" "$2" "$2" 200000 > build/real-values.tsv' sh "$ROOT/Xapiand" "$ROOT"
quiet light c1-check 10 'Validate benchmark input without timing' -- build/sortable_speed build/real-values.tsv 0
quiet exclusive c1-aa 10 'Codec A/A baseline' -- build/sortable_speed build/real-values.tsv 20
```

The exclusive command above is a follow-up command, not a completed measurement. At the
check, existing x7 light holds ran until 18:25, 18:50 and 22:20 local time. On macOS,
`quiet strict` also requires isolation from them. No other campaign was paused or stopped.
Repeat the baseline at the same commit to establish A/A spread before trying an optimization.

## Native A/A evidence and experiments

Three interleaved whole runs, each with one warmup and seven samples of 40 passes, measure
baseline, encoder switch and decoder switch. The production header remains identical to
`sortable:ac13b77`; candidates are generated by `bench/experiment.py` in isolated include
directories (`sortable:903d13f`). Raw samples are in `docs/perf/data/mac-*.tsv`.
Intervals below span the three run medians, rather than pooling sample timings.

| Set | Type | Encode ns/value, median [range] | Decode ns/value, median [range] | B/value |
|---|---|---:|---:|---:|
| uniform-8 | int64 | 4.035 [3.441, 4.075] | 13.863 [11.613, 13.934] | 1.9390 |
| uniform-8 | uint64 | 2.736 [2.549, 3.118] | 8.358 [7.944, 9.478] | 1.9650 |
| uniform-16 | int64 | 4.033 [3.690, 4.088] | 11.720 [11.431, 12.219] | 2.9346 |
| uniform-16 | uint64 | 2.616 [2.502, 2.629] | 8.500 [8.328, 8.893] | 2.9630 |
| uniform-32 | int64 | 4.803 [4.661, 4.913] | 12.189 [12.155, 12.491] | 4.9375 |
| uniform-32 | uint64 | 3.119 [3.079, 3.184] | 9.713 [9.067, 9.854] | 4.9613 |
| uniform-64 | int64 | 5.109 [5.082, 5.359] | 14.958 [14.957, 15.039] | 8.9402 |
| uniform-64 | uint64 | 4.461 [4.367, 4.480] | 12.587 [12.165, 12.628] | 8.9672 |
| uniform-binary64-bits | double | 6.650 [6.466, 6.682] | 19.090 [18.549, 21.681] | 9.8150 |
| accounts._id | int64 | 3.068 [3.044, 3.135] | 8.126 [8.111, 8.296] | 2.2340 |
| accounts._id | uint64 | 2.455 [2.376, 2.561] | 7.718 [7.505, 8.350] | 2.2340 |
| accounts._id | double | 4.046 [3.950, 4.070] | 10.129 [9.751, 10.331] | 2.2340 |
| accounts.accountNumber | int64 | 3.828 [3.821, 3.938] | 9.632 [9.618, 10.575] | 3.7390 |
| accounts.accountNumber | uint64 | 2.366 [2.361, 2.414] | 9.466 [9.382, 9.482] | 3.7390 |
| accounts.accountNumber | double | 5.185 [4.849, 5.350] | 11.201 [11.162, 11.332] | 3.7390 |
| accounts.age | int64 | 3.176 [3.165, 3.253] | 8.215 [8.099, 9.878] | 1.9530 |
| accounts.age | uint64 | 2.437 [2.381, 2.448] | 7.807 [7.646, 9.175] | 1.9530 |
| accounts.age | double | 4.122 [4.054, 4.244] | 9.663 [9.608, 10.275] | 1.9530 |
| accounts.balance | double | 5.694 [5.601, 7.938] | 11.795 [11.486, 15.045] | 7.8220 |
| cities._id | int64 | 3.000 [2.993, 3.154] | 7.954 [7.934, 8.727] | 2.2340 |
| cities._id | uint64 | 2.375 [2.372, 3.850] | 7.541 [7.523, 10.753] | 2.2340 |
| cities._id | double | 3.950 [3.949, 4.144] | 10.334 [10.327, 10.633] | 2.2340 |
| cities.population | int64 | 3.850 [3.683, 3.883] | 8.890 [8.713, 8.956] | 3.1070 |
| cities.population | uint64 | 2.393 [2.372, 2.474] | 8.249 [8.215, 10.369] | 3.1070 |
| cities.population | double | 4.463 [4.323, 4.677] | 10.319 [10.027, 11.317] | 3.1070 |
| cities coordinates as floating | double | 5.801 [5.775, 6.068] | 12.365 [12.312, 13.744] | 7.9425 |
| file sizes (bytes) | int64 | 4.454 [4.275, 4.487] | 9.168 [9.162, 9.373] | 2.1920 |
| file sizes (bytes) | uint64 | 3.392 [3.315, 3.516] | 9.121 [8.891, 9.518] | 2.1920 |
| file sizes (bytes) | double | 5.092 [4.974, 5.855] | 10.618 [10.437, 12.228] | 2.1920 |
| file mtimes (datetime, us) | double | 5.724 [5.712, 5.940] | 12.179 [11.664, 12.362] | 6.2704 |
| git commit times (datetime, s) | double | 5.360 [5.285, 5.828] | 11.021 [10.932, 12.002] | 4.9854 |

The complete encoder switch is dropped: on native macOS, unsigned uniform 64-bit input
improves from 4.461 to 3.069 ns/value (31.2% lower), but unsigned account numbers regress
from 2.366 to 4.952 ns/value (109.3% higher). The complete decoder switch is dropped:
unsigned file sizes regress from 9.121 to 11.448 ns/value (25.5% higher). Both candidates
pass the unchanged fixtures/exhaustive short-input test and the 15,807,814-value property
suite on macOS and Linux.

A third candidate uses the encoder switch only for tails of six or more bytes. The retention
rule is a nonoverlapping improvement interval for a relevant wide-value input on both
platforms, with no nonoverlapping regression interval on the real-value sets. The production
header stays unchanged unless that rule and the identity/fuzz checks pass.

The public old-codec probe links successfully against the existing Xapiand build's opaque
codec object and Cartesian library. One seeded NaN input decodes to 0.5 through the old
public API, so the shared comparison explicitly selects finite binary64 values. No old codec
implementation source is inspected. The public floating overload takes `long double`, so the
probe explicitly promotes double before calling it. Old/shared-finite and Linux isolated
timings are pending the host's existing light holds.
