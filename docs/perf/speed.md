# Sortable encode and decode speed

C1 is partial against D87's per-operation speed bar. Matching public string APIs at the same
O2 flags, sortable encodes faster on all 31 measured profiles. Decode parity is not established:
account numbers represented as double take 15.29 versus 14.18 ns/value (+7.9%), and actual
file mtimes take 16.19 versus 15.62 ns/value (+3.6%), both outside the observed A/A ranges.
Uniform finite doubles have a +16.1% median decode cost with overlapping ranges.

No optimization is retained. Three byte-preserving loop candidates improve some wide values
but regress real-value sets on macOS and Linux. The final `sortable.hh`, reference model,
vectors and reject fixture are identical to `sortable:ac13b77`. The after column reuses the
before observations because the final codec is unchanged; it is not an independent speedup.

## Method and scope

The benchmark and experiment sources are at `sortable:7686bd8`; the original benchmark is
`sortable:04fc571`, and the isolated switch generators are `sortable:903d13f` and
`sortable:cd663d1`. The old black box is the existing `Xapiand:c3f270ecc` build, linked through
its public header and compiled object. Its SHA-256 is
`d71192bcc024bfa24db7b4aba6dfab845c33de8a8b2b4ef44b4d26dae2426a85`.
No old codec implementation source, legacy Python GPL module, or Xapian source is inspected.

The host is an Intel Core i9-9980HK running x86_64 macOS 15.8.1. AppleClang
17.0.0.17000604 builds both public-API arms with the exact flags from the Xapiand
RelWithDebInfo build, including O2. Both return strings for encode. Native decode includes
strict decoding and typed conversion; old decode uses the corresponding public Unserialise
conversion. Integer inputs select the exact signed or unsigned overload. Double inputs are
explicitly promoted to long double because the public header exposes that floating overload.
The old public-header dependency also requires the existing Cartesian static library.

The allocation-free native encode API is measured separately in `mac-new-finite-*.tsv` at
matched O2, and in the CMake Release O3 optimization experiments. Do not substitute the O3
results for the matching-O2 public-API comparison.

Each arm warms once and reports seven samples per whole run. There are three interleaved
whole runs per comparison. Tables show the median of those run medians; A/A ranges span
those three medians, rather than pooling individual values or samples. Timing loops exclude
input generation and allocation of input storage. All timings use host `quiet exclusive`,
10 minutes maximum per hold; every hold completes successfully.

Uniform integers cover 8-, 16-, 32- and 64-bit widths (seed 611). The native random binary64
profile contains 10,000 arbitrary bit patterns. A seeded NaN decodes to 0.5 through the old
public API, so the shared old/new comparison explicitly filters to the same 9,996 finite
patterns. No other inputs are silently removed.

The real corpus uses the design branch's actual `sizes.py` generator and assets. It contains
434,977 original values: the same kinds of sets as the design's 430,166-value snapshot, with
26,977 current git commit times instead of 22,166. File metadata and git history are sampled
once. `data/real-values.tsv.gz` freezes that exact corpus; its uncompressed SHA-256 is
`7b5c20d8250470d05689afb95eadf0bef6a811fa5209ade7de972e8dfedcc012`.
Nonnegative real integers also run as uint64 and double. A double conversion above 2**53 can
round, so that arm is not an exact-integer substitute.

## Matched public APIs on macOS

Times are encode / decode ns/value; sizes are old / sortable bytes/value. The after values
are identical to the before values because all three candidates are dropped.

| Set | Type | Values | Old E / D | Sortable before E / D | Sortable after E / D | Old / sortable B |
|---|---|---:|---:|---:|---:|---:|
| uniform-8 | int64 | 10,000 | 37.88 / 19.43 | 7.46 / 14.30 | 7.46 / 14.30 | 2.1165 / 1.9390 |
| uniform-8 | uint64 | 10,000 | 34.20 / 19.64 | 5.50 / 12.08 | 5.50 / 12.08 | 2.2382 / 1.9650 |
| uniform-16 | int64 | 10,000 | 44.35 / 23.56 | 8.30 / 14.37 | 8.30 / 14.37 | 3.1068 / 2.9346 |
| uniform-16 | uint64 | 10,000 | 34.45 / 19.53 | 5.91 / 12.92 | 5.91 / 12.92 | 3.2386 / 2.9630 |
| uniform-32 | int64 | 10,000 | 40.04 / 22.25 | 9.79 / 15.63 | 9.79 / 15.63 | 5.1093 / 4.9375 |
| uniform-32 | uint64 | 10,000 | 35.47 / 20.91 | 7.22 / 13.45 | 7.22 / 13.45 | 5.2352 / 4.9613 |
| uniform-64 | int64 | 10,000 | 35.32 / 22.54 | 10.53 / 18.08 | 10.53 / 18.08 | 9.1086 / 8.9402 |
| uniform-64 | uint64 | 10,000 | 37.94 / 21.68 | 7.54 / 15.62 | 7.54 / 15.62 | 9.2403 / 8.9672 |
| uniform-finite-binary64-bits | double | 9,996 | 33.46 / 19.58 | 10.19 / 22.73 | 10.19 / 22.73 | 8.8608 / 9.8185 |
| accounts._id | int64 | 1,000 | 33.29 / 16.50 | 7.15 / 11.26 | 7.15 / 11.26 | 2.6800 / 2.2340 |
| accounts._id | uint64 | 1,000 | 32.25 / 17.13 | 5.22 / 11.66 | 5.22 / 11.66 | 2.6800 / 2.2340 |
| accounts._id | double | 1,000 | 30.98 / 13.87 | 6.70 / 13.45 | 6.70 / 13.45 | 2.6800 / 2.2340 |
| accounts.accountNumber | int64 | 1,000 | 32.27 / 16.09 | 10.10 / 13.20 | 10.10 / 13.20 | 3.9440 / 3.7390 |
| accounts.accountNumber | uint64 | 1,000 | 32.02 / 17.47 | 8.22 / 13.98 | 8.22 / 13.98 | 3.9440 / 3.7390 |
| accounts.accountNumber | double | 1,000 | 31.91 / 14.18 | 9.46 / 15.29 | 9.46 / 15.29 | 3.9440 / 3.7390 |
| accounts.age | int64 | 1,000 | 32.91 / 16.03 | 7.22 / 11.15 | 7.22 / 11.15 | 2.0000 / 1.9530 |
| accounts.age | uint64 | 1,000 | 32.28 / 17.17 | 5.28 / 11.94 | 5.28 / 11.94 | 2.0000 / 1.9530 |
| accounts.age | double | 1,000 | 32.58 / 13.84 | 6.79 / 13.48 | 6.79 / 13.48 | 2.0000 / 1.9530 |
| accounts.balance | double | 1,000 | 30.90 / 15.58 | 8.20 / 15.89 | 8.20 / 15.89 | 7.8250 / 7.8220 |
| cities._id | int64 | 1,000 | 31.94 / 16.16 | 7.22 / 11.38 | 7.22 / 11.38 | 2.6800 / 2.2340 |
| cities._id | uint64 | 1,000 | 30.87 / 17.98 | 5.24 / 11.66 | 5.24 / 11.66 | 2.6800 / 2.2340 |
| cities._id | double | 1,000 | 32.72 / 13.79 | 6.72 / 13.47 | 6.72 / 13.47 | 2.6800 / 2.2340 |
| cities.population | int64 | 1,000 | 34.05 / 17.32 | 8.67 / 11.98 | 8.67 / 11.98 | 3.6610 / 3.1070 |
| cities.population | uint64 | 1,000 | 31.52 / 18.55 | 6.21 / 12.58 | 6.21 / 12.58 | 3.6610 / 3.1070 |
| cities.population | double | 1,000 | 31.43 / 15.10 | 8.29 / 14.25 | 8.29 / 14.25 | 3.6610 / 3.1070 |
| cities coordinates as floating | double | 2,000 | 31.66 / 16.55 | 8.80 / 16.53 | 8.80 / 16.53 | 7.9880 / 7.9425 |
| file sizes (bytes) | int64 | 200,000 | 31.98 / 16.75 | 9.31 / 12.31 | 9.31 / 12.31 | 2.4226 / 2.1920 |
| file sizes (bytes) | uint64 | 200,000 | 32.52 / 18.14 | 6.73 / 12.93 | 6.73 / 12.93 | 2.4226 / 2.1920 |
| file sizes (bytes) | double | 200,000 | 31.44 / 14.77 | 8.54 / 14.47 | 8.54 / 14.47 | 2.4226 / 2.1920 |
| file mtimes (datetime, us) | double | 200,000 | 31.65 / 15.62 | 8.82 / 16.19 | 8.82 / 16.19 | 6.2922 / 6.2704 |
| git commit times (datetime, s) | double | 26,977 | 31.94 / 15.20 | 8.33 / 15.25 | 8.33 / 15.25 | 4.9964 / 4.9854 |

The decode rows requiring attention are:

| Set | Type | Old decode median [A/A range] | Sortable decode median [A/A range] | Median change |
|---|---|---:|---:|---:|
| accounts.accountNumber | double | 14.177 [13.586–14.608] | 15.293 [15.129–15.971] | +7.9% |
| file mtimes (datetime, us) | double | 15.624 [15.549–15.756] | 16.189 [15.989–16.439] | +3.6% |
| uniform-finite-binary64-bits | double | 19.582 [19.464–23.342] | 22.731 [22.596–23.636] | +16.1% |

The actual file-mtime decode difference is about 0.57 CPU-seconds per billion decoded values
in this microbenchmark. This is a CPU cost, not a measured service power or dollar estimate.

## Linux baseline and A/A spread

Ubuntu 24.04, kernel 6.8, GCC 13.3.0, CMake Release O3, in the existing Strata VM on port
18772: 8 vCPUs and 16 GiB under HVF on the same host. Host exclusive holds isolate these
measurements. Detent's VMs on ports 18762/18763 are untouched. A Linux old-codec black-box
comparison at a named Xapiand commit is not measured.

The codec is unchanged after the experiments. Intervals are the three whole-run medians.

| Set | Type | Encode ns [A/A range] | Decode ns [A/A range] | Sortable B |
|---|---|---:|---:|---:|
| uniform-8 | int64 | 6.880 [6.656–6.984] | 15.113 [14.483–15.296] | 1.9390 |
| uniform-8 | uint64 | 3.578 [3.506–3.599] | 9.046 [8.859–9.520] | 1.9650 |
| uniform-16 | int64 | 7.385 [7.053–7.494] | 15.714 [15.491–16.216] | 2.9346 |
| uniform-16 | uint64 | 3.677 [3.536–3.684] | 9.925 [9.507–9.934] | 2.9630 |
| uniform-32 | int64 | 7.939 [7.914–8.134] | 16.828 [16.560–17.429] | 4.9375 |
| uniform-32 | uint64 | 4.056 [3.984–4.149] | 10.446 [10.314–10.537] | 4.9613 |
| uniform-64 | int64 | 8.859 [8.814–9.295] | 17.534 [17.075–17.725] | 8.9402 |
| uniform-64 | uint64 | 5.435 [5.219–5.552] | 10.962 [10.743–11.124] | 8.9672 |
| uniform-binary64-bits | double | 15.459 [15.193–15.962] | 22.941 [22.625–23.284] | 9.8150 |
| accounts._id | int64 | 2.976 [2.909–3.049] | 11.812 [11.199–11.846] | 2.2340 |
| accounts._id | uint64 | 3.187 [3.179–3.293] | 8.430 [8.409–8.905] | 2.2340 |
| accounts._id | double | 3.889 [3.887–4.029] | 11.514 [11.509–11.979] | 2.2340 |
| accounts.accountNumber | int64 | 4.162 [4.154–4.324] | 16.312 [16.235–16.841] | 3.7390 |
| accounts.accountNumber | uint64 | 4.091 [4.021–4.209] | 12.218 [12.206–12.784] | 3.7390 |
| accounts.accountNumber | double | 6.977 [6.961–7.191] | 16.287 [15.795–16.788] | 3.7390 |
| accounts.age | int64 | 3.194 [3.190–3.282] | 10.953 [10.935–12.066] | 1.9530 |
| accounts.age | uint64 | 3.257 [3.254–3.396] | 8.733 [8.360–8.841] | 1.9530 |
| accounts.age | double | 4.313 [4.303–4.438] | 11.568 [11.467–11.938] | 1.9530 |
| accounts.balance | double | 6.856 [6.852–7.101] | 13.678 [13.556–14.942] | 7.8220 |
| cities._id | int64 | 2.986 [2.917–3.050] | 10.971 [10.960–11.219] | 2.2340 |
| cities._id | uint64 | 3.183 [3.161–3.273] | 8.767 [8.407–8.995] | 2.2340 |
| cities._id | double | 3.935 [3.918–3.988] | 11.745 [11.663–12.136] | 2.2340 |
| cities.population | int64 | 3.636 [3.631–3.729] | 12.801 [12.645–13.138] | 3.1070 |
| cities.population | uint64 | 3.421 [3.418–3.535] | 9.798 [9.765–10.046] | 3.1070 |
| cities.population | double | 4.590 [4.550–4.723] | 13.035 [12.740–13.737] | 3.1070 |
| cities coordinates as floating | double | 7.038 [6.874–7.556] | 13.314 [13.210–13.692] | 7.9425 |
| file sizes (bytes) | int64 | 4.612 [4.485–4.629] | 13.591 [13.169–13.839] | 2.1920 |
| file sizes (bytes) | uint64 | 4.551 [4.405–4.598] | 10.788 [10.364–10.883] | 2.1920 |
| file sizes (bytes) | double | 5.423 [5.340–5.660] | 14.224 [13.677–14.682] | 2.1920 |
| file mtimes (datetime, us) | double | 6.608 [6.490–6.618] | 14.558 [14.169–14.722] | 6.2704 |
| git commit times (datetime, s) | double | 5.782 [5.721–5.879] | 13.266 [13.205–13.382] | 4.9854 |

## Kept and dropped experiments

No change is kept. The retention rule requires a nonoverlapping improvement interval for a
relevant wide-value input on both platforms, with no nonoverlapping regression interval on
the real-value sets. The full switches fail on native macOS. Restricting the encoder switch
to tails of six or more bytes still fails on both platforms. Each candidate preserves the
same byte operations and passes the unchanged fixtures/exhaustive-short and property tests.

Representative operation costs are medians of three interleaved runs, before → candidate.
Every profile and sample is in the matching raw TSV files, including losses.

| Candidate | Platform | Profile | Operation | Before → candidate ns | Change | Verdict |
|---|---|---|---|---:|---:|---|
| Full encoder switch | mac | uniform-64, uint64 | Encode | 4.461 → 3.069 | -31.2% | Dropped |
| Full encoder switch | mac | accounts.accountNumber, uint64 | Encode | 2.366 → 4.952 | +109.3% | Dropped |
| Full encoder switch | linux | accounts.accountNumber, uint64 | Encode | 4.091 → 6.114 | +49.4% | Dropped |
| Full decoder switch | mac | file sizes (bytes), uint64 | Decode | 9.121 → 11.448 | +25.5% | Dropped |
| Full decoder switch | linux | file sizes (bytes), uint64 | Decode | 10.788 → 11.871 | +10.0% | Dropped |
| Long-tail encoder switch | mac | accounts.balance, double | Encode | 6.433 → 5.179 | -19.5% | Dropped |
| Long-tail encoder switch | linux | accounts.balance, double | Encode | 6.829 → 5.149 | -24.6% | Dropped |
| Long-tail encoder switch | mac | accounts.age, int64 | Encode | 4.083 → 4.836 | +18.4% | Dropped |
| Long-tail encoder switch | linux | accounts.age, int64 | Encode | 3.210 → 3.873 | +20.7% | Dropped |

The long-tail switch's age-int64 regression has nonoverlapping run ranges on both platforms:
macOS 3.443–4.086 → 4.158–4.845 ns and Linux 3.192–3.318 → 3.451–3.890 ns.
The performance investigation stops after three rejected candidates; decode parity remains
work for a different mechanism. This is a measured miss, not a retained speedup.

## Verification

1. Native and Linux Release builds pass all five CTests: core, property, sizes, fixture
   identity, and C++ CLI against the independent model.
2. Each of the three candidates on each platform passes all 113,491 vectors, 39 reject
   witnesses, the 83,951,873-input exhaustive-short check, and the 15,807,814-value property
   suite (including 15,807,804 ordered pairs). No fixture is modified.
3. The original standalone fuzz driver replays all 190 native corpus files successfully.
4. Ruff is clean and Pyrefly strict reports zero errors for the Python benchmark tools.
5. [Final-code CI run 43570100141](https://github.com/Kronuz/sortable/actions/runs/43570100141)
   passes at `sortable:7686bd8`: Linux x86_64 GCC/Clang and arm64, macOS x86_64/arm64,
   FreeBSD, ASan/UBSan and TSan. Subsequent changes contain only report data and documentation.

GCC 13 emits a possible buffer-overflow warning for the original header's variable tail
loop, and for the generated switches. This warning is present in the untouched baseline;
this report does not attribute it to a retained change or claim it proves an actual overflow.

## Reproduction

Run from the sortable checkout. Every build/test uses `quiet light`, at most four jobs;
every timing uses `quiet exclusive`, at most 10 minutes. `PASSES` is 40 for the final rows.
The original native O3 exploratory A/A files use the same loop and settings except the older
three baseline-only runs, which are not included in `data/` or the final comparison.

```sh
quiet check
quiet light c1-build 10 'Build codec and benchmark' -- sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4'
quiet light c1-tests 10 'Codec CTest' -- ctest --test-dir build -j4 --output-on-failure
quiet light c1-replay 10 'Replay existing corpus' -- sh -c 'build/sortable_standalone_fuzz fuzz/corpus/*'
gzip -dc docs/perf/data/real-values.tsv.gz > build/real-values.tsv
```

To generate a new real-value snapshot rather than replay the frozen one:

```sh
ROOT=/Users/kronuz/code/Kronuz
git -C "$ROOT/Xapiand" show design/sortable-encoding:docs/design/sortable-encoding/sizes.py > build/design-sizes.py
quiet light c1-corpus 10 'Export actual design values' -- sh -c 'python3 -I bench/export_values.py build/design-sizes.py "$1/docs/public/assets" "$2" "$2" 200000 > build/real-values.tsv' sh "$ROOT/Xapiand" "$ROOT"
```

The public-API build and matching timed comparison use the existing Xapiand build:

```sh
XBUILD=/Users/kronuz/code/Kronuz/.scratch/codex/idle-wakeups/build
quiet light c1-old-build 5 'Build old public probe' -- python3 -I bench/build_old.py "$XBUILD" build/old_speed old
quiet light c1-new-build 5 'Match O2 native string API' -- python3 -I bench/build_old.py "$XBUILD" build/sortable_speed_string_O2 string
quiet light c1-char-build 5 'Match O2 native char API' -- python3 -I bench/build_old.py "$XBUILD" build/sortable_speed_O2 new
quiet light c1-old-check 5 'Verify finite old-codec round trips' -- build/old_speed build/real-values.tsv 0 finite
quiet exclusive c1-public 10 'Three interleaved public-API runs' -- sh -c 'set -e; for run in 1 2 3; do build/old_speed build/real-values.tsv 40 finite > build/mac-public-old-$run.tsv; build/sortable_speed_string_O2 build/real-values.tsv 40 finite > build/mac-public-new-$run.tsv; done'
python3 -I bench/summarize.py build/mac-public-old-1.tsv build/mac-public-old-2.tsv build/mac-public-old-3.tsv
python3 -I bench/summarize.py build/mac-public-new-1.tsv build/mac-public-new-2.tsv build/mac-public-new-3.tsv
```

The allocation-free comparison uses the same interleaving, replacing the native binary with
`build/sortable_speed_O2`. Its actual hold is `codex-sortable-old-O2-time`; the public-string
hold is `codex-sortable-public-string-time`. Both complete normally.

Generate and compile the candidate include directories without modifying the source header:

```sh
for candidate in encode-switch decode-switch encode-long-switch; do
  mkdir -p "build/$candidate"
  python3 -I bench/experiment.py "$candidate" > "build/$candidate/sortable.hh"
  c++ -std=c++20 -O3 -DNDEBUG -I"build/$candidate" -I. bench/speed.cc -o "build/$candidate/speed"
done
```

Candidate correctness uses the same `-Ibuild/CANDIDATE -I.` order to compile `test/test.cc`
(with `-pthread` and `-DSORTABLE_TEST_DIR` pointing to the absolute fixture directory) and
`test/property.cc`, followed by running both binaries under `quiet light`. The existing
fixtures and model stay in the original source checkout.

The exact native interleaving for the full switches is:

```sh
quiet exclusive c1-mac-switches 10 'Three interleaved switch runs' -- sh -c 'set -e; for run in 1 2 3; do build/sortable_speed build/real-values.tsv 40 > build/mac-baseline-$run.tsv; build/encode-switch/speed build/real-values.tsv 40 > build/mac-encode-switch-$run.tsv; build/decode-switch/speed build/real-values.tsv 40 > build/mac-decode-switch-$run.tsv; done'
```

For the long-tail candidate, interleave `build/sortable_speed` and
`build/encode-long-switch/speed` three times, using the same corpus and 40 passes. Its raw
prefixes are `mac-long-baseline` and `mac-long-encode-long-switch`.

The Linux build copies the same CMake file, header, benchmark, tests, tools and fuzz tree into
`/home/strata/.scratch/codex-sortable-speed`, then runs CMake Release, `cmake --build ... -j4`
and CTest under a host `quiet light` hold. Copy the frozen TSV and generated candidate headers
into that build and compile candidates with `g++ -std=c++20 -O3 -DNDEBUG`, using the same
include ordering and correctness commands. Timing uses the same loop and pass count through
this host wrapper; the actual holds are `codex-sortable-linux-time` and
`codex-sortable-long-time`:

```sh
quiet exclusive c1-linux 10 'Linux VM benchmark' -- ssh -i /Users/kronuz/code/Kronuz/strata/.scratch/linux-vm/ssh_key -o UserKnownHostsFile=/Users/kronuz/code/Kronuz/strata/.scratch/linux-vm/known_hosts -p 18772 strata@127.0.0.1 'set -e; root=/home/strata/.scratch/codex-sortable-speed; for run in 1 2 3; do for arm in baseline encode-switch decode-switch; do if test "$arm" = baseline; then binary=$root/build/sortable_speed; else binary=$root/build/$arm/speed; fi; "$binary" "$root/build/real-values.tsv" 40 > "$root/build/linux-$arm-$run.tsv"; done; done'
```

For each platform, `bench/summarize.py` on the three raw files reproduces its whole-run
median and A/A table. The long-tail before/after files use their own paired baseline,
not a baseline from another hold. The full raw samples, including overlapping intervals and
regressions, are committed beside this report.
