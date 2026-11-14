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
All passed. Ruff and Pyrefly strict checked the Python exporter. A fuzz-corpus replay is
not yet run; the codec implementation is byte-for-byte unchanged.

From the sortable checkout, with `ROOT=/Users/kronuz/code/Kronuz`:

```sh
quiet check
quiet light c1-build 10 'Build codec benchmark' -- sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4'
quiet light c1-tests 10 'Codec tests' -- ctest --test-dir build -j4 --output-on-failure
git -C "$ROOT/Xapiand" show design/sortable-encoding:docs/design/sortable-encoding/sizes.py > build/design-sizes.py
quiet light c1-corpus 10 'Export actual design values' -- sh -c 'python3 -I bench/export_values.py build/design-sizes.py "$1/docs/public/assets" "$2" "$2" 200000 > build/real-values.tsv' sh "$ROOT/Xapiand" "$ROOT"
quiet light c1-check 10 'Validate benchmark input without timing' -- build/sortable_speed build/real-values.tsv 0
quiet exclusive c1-aa 10 'Codec A/A baseline' -- build/sortable_speed build/real-values.tsv 20
```

The exclusive command above is a follow-up command, not a completed measurement. At the
check, existing x7 light holds ran until 18:25, 18:50 and 22:20 local time. On macOS,
`quiet strict` also requires isolation from them. No other campaign was paused or stopped.
Repeat the baseline at the same commit to establish A/A spread before trying an optimization.

## Experiments and remaining evidence

No speed experiment is accepted or rejected without numbers. A bulk tail copy and a bounded
word-load decoder are candidates only; neither is implemented. Next, link a tiny probe against
a Xapiand build and call only the public `Serialise::serialise(double)` and matching
`Unserialise` entry point. Measure the same double-valued corpus without reading the old
codec implementation; its source was not opened here. Then run interleaved before/after pairs
and the unchanged fixtures, property test and fuzz corpus on x86_64 macOS and Linux. Keep a
change only when both platforms clear their own A/A band.
