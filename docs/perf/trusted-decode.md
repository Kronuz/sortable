# Trusted typed decode

The stored-value path exposes `to_double`, `to_int64`, and `to_uint64` on encoded bytes. It
retains the empty-input, escape-field, and eight-byte-tail bounds checks. Canonical checks stay
in `decode`, which is unchanged. On bytes written by `encode`, each typed result equals the
corresponding strict conversion, including nearest-even rounding and integer range failures.
For other bytes the trusted result is unspecified; the parser still stays within its input and
the conversions contain no undefined shift or narrowing.

## Correctness evidence

The fixture test compares all 113,491 vectors against strict decode for all three typed results.
The property test compares randomly generated encodings and every boundary value. The saved
`build/Testing/Temporary/LastTest.log` (November 14, 2026, 16:40 PST) records all five CTests
passing, including 39 reject witnesses and 15,807,814 property values.

The ASan and UBSan libFuzzer target calls all three trusted conversions on arbitrary bytes and
compares with strict decode whenever strict accepts. Build job `261114-180659-23c25e` passes at
sortable:2299f14b67a86a1ffbe152628b469d4733c77ada with Homebrew Clang 23.1.2,
RelWithDebInfo O2, `-fsanitize=fuzzer,address,undefined` and
`-fno-sanitize-recover=all`. Three sequential `quiet light` jobs at that exact commit exercise
674,417,247 inputs with no sanitizer finding. Their measured user plus system CPU time totals
3,801.51 seconds (1.056 CPU hours), exceeding the one CPU-hour requirement; this excludes the
separate 31-second smoke.

| Job | Exit | Executed inputs | User CPU s | System CPU s | CPU total s | Peak RSS MiB |
|---|---:|---:|---:|---:|---:|---:|
| `261114-180702-9b9ba9` | 0 | 308,586,260 | 1,698.73 | 1.52 | 1,700.25 | 172 |
| `261114-180704-071917` | 0 | 306,128,880 | 1,699.08 | 1.28 | 1,700.36 | 173 |
| `261114-180706-82c023` | 0 | 59,702,107 | 400.51 | 0.39 | 400.90 | 166 |
| Total | | 674,417,247 | 3,798.32 | 3.19 | 3,801.51 | 173 maximum |

Final libFuzzer coverage is 388 edges and 1,114 features. The failure-artifact directory is
empty. Each job's complete output, final statistics and `/usr/bin/time -p` counters are in
`~/.scratch/campaign/jobs/<job-id>/a1/log`; its job record binds the exit and commit. The jobs
share a writable corpus in the `codex-sortable-trusted-fuzz` slot, use seed 20261114, maximum
input length 4096 and per-input timeout 5 seconds, with
`ASAN_OPTIONS=quarantine_size_mb=64:malloc_context_size=5`.

## Benchmark method

The 31 profile set is the frozen 434,977-value corpus from `docs/perf/data/real-values.tsv.gz`;
see `docs/perf/speed.md` for the sets and provenance. The old codec is linked only through its
public Xapiand API and an existing compiled object. The macOS benchmark compiles all four arms
with the same Xapiand RelWithDebInfo O2 flags. Each arm warms once and takes seven samples per
whole run. Three whole runs interleave old, strict, safety-only, and trusted. Tables report the
median of run medians; the bracketed range is the span of those run medians. The safety-only arm
uses the trusted parser to construct a `number`, then the same typed conversion as strict decode;
it isolates the cost of canonical validation. The trusted arm calls the public typed byte API.
Input generation and input-storage allocation occur outside timed loops. The old codec returns a scalar;
the native integer APIs return an optional, so absolute old-versus-native cost includes that API
difference. The double arm escapes the same scalar in every timed loop.

The host is an Intel Core i9-9980HK running x86_64 macOS 15.8.1, AppleClang 17.0.0.17000604.
The Linux VM is Ubuntu 24.04, kernel 6.8, GCC 13.3.0, CMake-equivalent Release O3, 8 vCPUs and
16 GiB under HVF. Every timing has a host `quiet exclusive` hold of at most ten minutes. The
binary format, fixtures, and strict decoder are unchanged.

## macOS O2 results

The first four-arm timing used different result handling for trusted doubles and is invalid for
comparison. The table below is the corrected, fully rerun comparison.

| Set | Type | Old ns [A/A] | Strict ns [A/A] | Safety ns [A/A] | Trusted ns [A/A] | Trusted vs old |
|---|---|---:|---:|---:|---:|---:|
| uniform-8 | int64 | 19.50 [19.37, 20.60] | 14.05 [14.01, 14.06] | 9.87 [9.81, 10.24] | 10.46 [10.43, 10.46] | -46.4% |
| uniform-8 | uint64 | 19.25 [18.92, 19.26] | 12.39 [12.20, 13.65] | 6.98 [6.60, 7.10] | 6.60 [6.59, 6.70] | -65.7% |
| uniform-16 | int64 | 19.34 [19.04, 19.60] | 14.78 [14.34, 15.91] | 10.38 [10.27, 10.59] | 10.95 [10.70, 11.18] | -43.4% |
| uniform-16 | uint64 | 19.52 [18.92, 20.21] | 12.65 [12.64, 12.76] | 7.35 [7.25, 7.73] | 7.57 [7.47, 7.69] | -61.2% |
| uniform-32 | int64 | 20.88 [20.80, 21.16] | 15.72 [15.59, 15.78] | 11.42 [11.41, 11.54] | 11.62 [11.58, 11.64] | -44.4% |
| uniform-32 | uint64 | 20.40 [20.35, 21.04] | 13.40 [13.24, 13.58] | 8.23 [8.18, 9.86] | 8.52 [8.38, 8.57] | -58.3% |
| uniform-64 | int64 | 22.20 [21.75, 22.42] | 18.41 [18.19, 18.63] | 14.99 [14.88, 15.99] | 14.59 [14.44, 14.67] | -34.3% |
| uniform-64 | uint64 | 22.31 [21.39, 22.35] | 15.58 [15.43, 16.95] | 10.65 [10.44, 11.36] | 11.02 [10.83, 11.34] | -50.6% |
| uniform-finite-binary64-bits | double | 19.81 [19.39, 19.96] | 22.85 [22.61, 23.02] | 20.08 [20.08, 20.11] | 20.25 [19.61, 22.63] | +2.2% |
| accounts._id | int64 | 16.37 [16.17, 17.10] | 11.50 [11.02, 12.22] | 5.96 [5.83, 6.09] | 5.83 [5.75, 5.89] | -64.4% |
| accounts._id | uint64 | 18.25 [17.18, 28.44] | 11.46 [11.19, 11.96] | 6.54 [6.40, 6.83] | 6.35 [6.28, 6.40] | -65.2% |
| accounts._id | double | 13.88 [13.60, 21.26] | 13.65 [13.52, 14.17] | 8.38 [8.10, 8.47] | 7.97 [7.95, 8.13] | -42.6% |
| accounts.accountNumber | int64 | 15.63 [15.53, 15.80] | 13.37 [12.94, 13.86] | 8.29 [8.09, 8.32] | 8.15 [8.01, 8.18] | -47.9% |
| accounts.accountNumber | uint64 | 17.27 [16.72, 17.31] | 13.41 [13.39, 14.84] | 8.68 [8.57, 8.77] | 8.89 [8.79, 9.05] | -48.5% |
| accounts.accountNumber | double | 13.86 [13.58, 13.87] | 16.22 [16.00, 20.26] | 10.85 [10.68, 10.86] | 10.23 [10.16, 10.37] | -26.2% |
| accounts.age | int64 | 15.79 [15.70, 16.01] | 11.13 [11.12, 12.10] | 6.05 [6.04, 6.07] | 5.97 [5.84, 6.35] | -62.2% |
| accounts.age | uint64 | 16.84 [16.47, 17.76] | 11.42 [11.42, 14.84] | 6.72 [6.65, 6.85] | 6.18 [6.18, 6.32] | -63.3% |
| accounts.age | double | 13.28 [13.26, 13.87] | 13.50 [13.27, 14.09] | 8.48 [8.46, 8.59] | 8.19 [8.19, 8.39] | -38.3% |
| accounts.balance | double | 15.45 [14.94, 15.60] | 15.74 [15.74, 19.18] | 11.51 [10.92, 13.41] | 10.70 [10.45, 10.84] | -30.8% |
| cities._id | int64 | 16.74 [16.34, 17.17] | 11.02 [11.02, 11.90] | 5.92 [5.87, 5.94] | 5.79 [5.76, 5.89] | -65.4% |
| cities._id | uint64 | 17.52 [17.06, 18.57] | 12.24 [11.45, 14.97] | 6.54 [6.40, 6.57] | 6.27 [6.26, 6.39] | -64.2% |
| cities._id | double | 13.69 [13.61, 14.19] | 13.17 [13.16, 14.50] | 8.36 [8.18, 9.10] | 8.14 [7.98, 8.53] | -40.5% |
| cities.population | int64 | 17.59 [17.42, 20.36] | 12.14 [11.71, 12.42] | 6.82 [6.67, 6.94] | 6.81 [6.67, 6.85] | -61.3% |
| cities.population | uint64 | 18.88 [18.73, 19.56] | 13.10 [12.39, 13.76] | 7.39 [7.23, 7.50] | 7.63 [7.49, 7.65] | -59.6% |
| cities.population | double | 15.40 [15.10, 15.66] | 14.07 [14.05, 16.14] | 9.13 [9.07, 9.13] | 8.95 [8.80, 8.98] | -41.9% |
| cities coordinates as floating | double | 16.19 [15.74, 16.74] | 16.21 [15.87, 17.25] | 11.90 [11.52, 12.26] | 11.35 [11.26, 11.47] | -29.9% |
| file sizes (bytes) | int64 | 17.09 [16.57, 17.22] | 12.48 [12.23, 12.75] | 7.51 [7.37, 7.62] | 7.36 [7.29, 7.56] | -56.9% |
| file sizes (bytes) | uint64 | 18.36 [18.09, 18.53] | 12.82 [12.78, 12.86] | 7.80 [7.77, 8.17] | 7.98 [7.86, 8.07] | -56.5% |
| file sizes (bytes) | double | 14.56 [14.46, 14.78] | 14.63 [14.55, 15.03] | 9.82 [9.63, 9.91] | 9.79 [9.68, 9.84] | -32.8% |
| file mtimes (datetime, us) | double | 15.87 [15.63, 16.30] | 16.19 [16.11, 16.21] | 11.41 [11.31, 11.43] | 10.72 [10.57, 10.83] | -32.4% |
| git commit times (datetime, s) | double | 15.01 [14.95, 15.20] | 15.36 [15.00, 15.55] | 9.95 [9.81, 10.35] | 9.80 [9.80, 9.97] | -34.7% |

The trusted arm beats the old arm outside its A/A range on 30 of 31 macOS profiles.
For account-number doubles, strict to safety-only removes 5.37 ns/value
(16.22 → 10.85); trusted is 10.23. For file mtimes, the corresponding values are
16.19 → 11.41 → 10.72 ns/value. The validation work accounts for the former gap.

The uniform finite binary64 row overlaps the old codec's A/A range. An additional three-run,
400-pass comparison gives old 19.906 [19.786, 22.125] and trusted 20.168 [19.967, 22.293]
ns/value. That difference is unresolved at the observed noise level.

## Linux O3 results

The Linux old public probe is compiled from the unchanged Xapiand:c3f270ecc codec source
through the public API and a dependency closure generated from its existing macOS build.
The closure script packages files without inspecting the codec implementation. All four
probes use GCC 13.3.0 O3. The old probe passes every finite corpus round trip before timing.
This is a standalone Linux build with copied generated headers and definitions, not a full
Linux Xapiand CMake build; the measured old API behavior is verified on this corpus only.

| Set | Type | Old ns [A/A] | Strict ns [A/A] | Safety ns [A/A] | Trusted ns [A/A] | Trusted vs old |
|---|---|---:|---:|---:|---:|---:|
| uniform-8 | int64 | 23.77 [23.51, 24.69] | 11.08 [10.97, 11.24] | 8.28 [7.77, 8.49] | 8.17 [8.16, 8.48] | -65.6% |
| uniform-8 | uint64 | 23.80 [23.58, 24.43] | 8.76 [8.53, 8.84] | 5.37 [5.31, 5.54] | 5.13 [4.98, 5.16] | -78.4% |
| uniform-16 | int64 | 23.80 [23.56, 25.14] | 11.46 [11.34, 12.49] | 8.04 [7.88, 8.50] | 8.59 [8.56, 8.81] | -63.9% |
| uniform-16 | uint64 | 21.25 [21.15, 21.29] | 8.85 [8.55, 8.95] | 5.63 [5.44, 5.66] | 5.40 [5.25, 5.46] | -74.6% |
| uniform-32 | int64 | 24.63 [24.19, 24.65] | 12.53 [12.24, 13.00] | 8.12 [8.05, 8.27] | 8.94 [8.82, 8.94] | -63.7% |
| uniform-32 | uint64 | 21.97 [21.57, 22.19] | 9.55 [9.41, 9.75] | 5.92 [5.80, 6.00] | 5.71 [5.58, 5.79] | -74.0% |
| uniform-64 | int64 | 27.33 [27.24, 28.00] | 13.02 [12.77, 13.57] | 9.27 [9.14, 9.71] | 9.52 [9.52, 9.64] | -65.2% |
| uniform-64 | uint64 | 27.01 [26.31, 39.29] | 11.13 [10.44, 11.21] | 6.42 [6.36, 6.44] | 7.42 [6.99, 8.01] | -72.5% |
| uniform-finite-binary64-bits | double | 28.41 [27.83, 41.88] | 20.77 [19.78, 21.13] | 12.94 [12.78, 13.35] | 13.11 [13.10, 13.25] | -53.8% |
| accounts._id | int64 | 19.57 [19.17, 29.85] | 6.71 [6.71, 12.52] | 4.02 [3.94, 4.02] | 5.11 [5.11, 5.23] | -73.9% |
| accounts._id | uint64 | 19.39 [19.27, 30.00] | 8.16 [8.15, 10.43] | 4.96 [4.95, 5.65] | 4.78 [4.67, 4.89] | -75.4% |
| accounts._id | double | 16.12 [15.95, 25.72] | 9.97 [9.97, 14.56] | 7.23 [6.42, 11.00] | 6.82 [6.66, 7.00] | -57.7% |
| accounts.accountNumber | int64 | 19.36 [18.34, 30.22] | 12.71 [11.43, 16.26] | 5.87 [5.79, 6.41] | 6.78 [6.65, 6.97] | -65.0% |
| accounts.accountNumber | uint64 | 19.28 [18.95, 29.06] | 10.09 [10.05, 13.86] | 5.99 [5.94, 6.04] | 5.60 [5.60, 5.71] | -70.9% |
| accounts.accountNumber | double | 17.55 [16.57, 20.44] | 12.84 [12.82, 15.94] | 8.15 [7.95, 9.75] | 7.96 [7.94, 8.11] | -54.6% |
| accounts.age | int64 | 17.51 [17.45, 21.69] | 10.40 [7.56, 12.33] | 5.06 [4.09, 6.21] | 5.25 [5.23, 5.34] | -70.0% |
| accounts.age | uint64 | 18.75 [18.54, 18.78] | 11.36 [8.24, 11.47] | 5.02 [4.93, 5.29] | 4.84 [4.84, 4.94] | -74.2% |
| accounts.age | double | 15.59 [14.94, 15.94] | 12.92 [10.42, 13.02] | 6.59 [6.44, 7.09] | 6.93 [6.78, 7.74] | -55.6% |
| accounts.balance | double | 18.89 [18.14, 18.93] | 12.64 [11.59, 13.59] | 8.93 [7.61, 8.95] | 8.22 [8.03, 9.35] | -56.5% |
| cities._id | int64 | 19.06 [18.77, 19.19] | 6.73 [6.71, 7.75] | 4.62 [3.93, 5.04] | 5.11 [5.10, 5.22] | -73.2% |
| cities._id | uint64 | 19.02 [18.91, 19.43] | 8.16 [8.16, 13.17] | 5.30 [4.95, 5.34] | 4.67 [4.67, 4.78] | -75.4% |
| cities._id | double | 15.70 [15.68, 17.21] | 10.08 [9.87, 13.66] | 6.41 [6.28, 6.42] | 6.67 [6.66, 6.82] | -57.5% |
| cities.population | int64 | 20.68 [20.61, 21.06] | 8.40 [7.91, 11.34] | 4.77 [4.66, 4.77] | 5.78 [5.75, 5.92] | -72.1% |
| cities.population | uint64 | 20.93 [20.28, 21.07] | 8.75 [8.56, 10.02] | 5.36 [5.24, 5.39] | 5.16 [5.14, 5.26] | -75.3% |
| cities.population | double | 17.82 [17.73, 18.38] | 13.06 [10.76, 13.47] | 7.03 [7.02, 7.58] | 7.28 [7.28, 7.43] | -59.1% |
| cities coordinates as floating | double | 20.62 [19.29, 20.97] | 11.76 [11.55, 12.62] | 8.17 [8.01, 8.39] | 8.64 [8.53, 8.75] | -58.1% |
| file sizes (bytes) | int64 | 19.36 [19.34, 19.84] | 9.11 [9.07, 9.39] | 5.54 [5.31, 5.57] | 6.43 [6.36, 6.59] | -66.8% |
| file sizes (bytes) | uint64 | 19.35 [19.28, 19.58] | 9.52 [9.51, 9.60] | 6.04 [6.01, 6.08] | 5.89 [5.79, 5.91] | -69.5% |
| file sizes (bytes) | double | 17.07 [16.91, 17.14] | 11.43 [11.26, 11.49] | 7.89 [7.88, 7.98] | 8.00 [7.89, 8.05] | -53.2% |
| file mtimes (datetime, us) | double | 18.43 [18.15, 18.97] | 12.45 [12.25, 12.52] | 8.19 [8.12, 8.30] | 8.52 [8.26, 8.61] | -53.8% |
| git commit times (datetime, s) | double | 16.79 [16.70, 17.56] | 11.84 [11.72, 11.85] | 7.65 [7.62, 8.73] | 7.90 [7.83, 9.41] | -53.0% |

The trusted arm beats the old arm outside its A/A range on all 31 Linux profiles.
For account-number doubles, strict to safety-only removes 4.69 ns/value; trusted adds
no meaningful cost (12.84 → 8.15 → 7.96). For file mtimes, the corresponding values
are 12.45 → 8.19 → 8.52 ns/value. The safety-only and trusted arms sometimes differ
in either direction because the compiler specializes their call shapes differently;
the validation cost is the robust strict-to-safety difference.

## Reproduction

Raw timings and the 434,977-value corpus are kept in history, not in the tree: restore
them with `git archive d927b7e docs/perf/data | tar -x` before the commands below. Build the
bench with `-DSORTABLE_BENCH=ON`.

Run the local suite under a light hold with four build jobs. For the sanitizer fuzz target use
Homebrew LLVM on macOS, and give libFuzzer a writable copy of `fuzz/corpus` under ignored
`build/`. The fuzzer mutates its corpus directory.

```sh
quiet check
quiet light c10-build 10 'Build trusted sortable tests and benches' -- cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
quiet light c10-compile 10 'Compile trusted sortable tests and benches' -- cmake --build build -j 4
quiet light c10-ctest 10 'Verify trusted parity and existing sortable suite' -- ctest --test-dir build --output-on-failure -j 4
quiet light c10-fuzz-build 10 'Build ASan UBSan trusted decoder fuzzer' -- sh -c 'cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSORTABLE_FUZZ=ON && cmake --build build-fuzz --target sortable_fuzz -j 4'
```

The completed fuzz jobs use the command below, with `FUZZ_SECONDS` set to 1700, 1700 and
400 in sequence under light holds of 30, 30 and 10 minutes respectively. `/usr/bin/time`
reports actual CPU separately from the fuzzer's wall-time limit. Continue only if all chunks
exit zero, and add a bounded chunk if their successful user plus system time is below 3600.

```sh
ASAN_OPTIONS=quarantine_size_mb=64:malloc_context_size=5 /usr/bin/time -p \
  build-fuzz/sortable_fuzz build-fuzz/corpus \
  -artifact_prefix=build-fuzz/artifacts/ -seed=20261114 \
  -max_total_time="$FUZZ_SECONDS" -timeout=5 -max_len=4096 -print_final_stats=1
```

For the macOS public API timing, `XBUILD` is the existing Xapiand build at
`/Users/kronuz/code/Kronuz/.scratch/codex/idle-wakeups/build`. The build helper reads only
compiler flags and links the compiled codec object; it does not inspect that codec's source.

```sh
XBUILD=/Users/kronuz/code/Kronuz/.scratch/codex/idle-wakeups/build
gzip -dc docs/perf/data/real-values.tsv.gz > build/real-values.tsv
quiet light c10-probe-build 10 'Build matched old strict safety trusted probes' -- sh -c 'set -e; for mode in old string safety trusted; do /usr/local/bin/python3 -I bench/build_old.py /Users/kronuz/code/Kronuz/.scratch/codex/idle-wakeups/build build/probe-$mode $mode; file build/probe-$mode; done'
quiet exclusive c10-mac-fair 10 'Fair interleaved old strict safety trusted decode' -- sh -c 'set -e; for run in 1 2 3; do for arm in old string safety trusted; do build/probe-$arm build/real-values.tsv 40 finite > build/mac-c10b-$arm-$run.tsv; done; done'
/usr/local/bin/python3 -I bench/summarize_decode.py build/mac-c10b
quiet exclusive c10-mac-finite 10 'Resolve finite binary64 trusted parity A/A' -- sh -c 'set -e; for run in 1 2 3; do for arm in old trusted; do build/probe-$arm - 400 finite > build/mac-finite-$arm-$run.tsv; done; done'
```

The Linux benchmark uses the existing VM on port 18772. `bench/build_linux_old.py` builds a
tarball from the already-compiled old object's dependency closure and writes the O3 build script;
it reads the public probe's includes but never inspects the old implementation. The source and
headers are the unchanged codec from Xapiand:c3f270ecc. The macOS compiled object used above has
SHA-256 `d71192bcc024bfa24db7b4aba6dfab845c33de8a8b2b4ef44b4d26dae2426a85`.
The Linux commands, from this repository, are:

```sh
XBUILD=/Users/kronuz/code/Kronuz/.scratch/codex/idle-wakeups/build
VM=/home/strata/.scratch/codex-sortable-trusted
KEY=/Users/kronuz/code/Kronuz/strata/.scratch/linux-vm/ssh_key
KNOWN=/Users/kronuz/code/Kronuz/strata/.scratch/linux-vm/known_hosts
quiet light c10-closure-tool 3 'Package old public codec closure' -- /usr/local/bin/python3 -I bench/build_linux_old.py "$XBUILD" "$VM" build
quiet light c10-vm-stage 3 'Stage Linux probes' -- sh -c 'ssh -i "$1" -o UserKnownHostsFile="$2" -p 18772 strata@127.0.0.1 "mkdir -p $3/bench $3/build $3/old-src"; scp -q -i "$1" -o UserKnownHostsFile="$2" -P 18772 sortable.hh bench/speed.cc build/linux-old-closure.tar.gz build/linux-old-build.sh strata@127.0.0.1:$3/build/; ssh -i "$1" -o UserKnownHostsFile="$2" -p 18772 strata@127.0.0.1 "mv $3/build/sortable.hh $3/sortable.hh; mv $3/build/speed.cc $3/bench/speed.cc; tar -xzf $3/build/linux-old-closure.tar.gz -C $3/old-src"' sh "$KEY" "$KNOWN" "$VM"
quiet light c10-vm-build 10 'Build four Linux decoder probes' -- ssh -i "$KEY" -o UserKnownHostsFile="$KNOWN" -p 18772 strata@127.0.0.1 'set -e; root=/home/strata/.scratch/codex-sortable-trusted; sh "$root/build/linux-old-build.sh"; for arm in string safety trusted; do case "$arm" in string) flags="";; safety) flags="-DSORTABLE_SAFETY_CODEC";; trusted) flags="-DSORTABLE_TRUSTED_CODEC";; esac; g++ -O3 -DNDEBUG -std=c++20 -DSORTABLE_STRING_CODEC $flags -I "$root" "$root/bench/speed.cc" -o "$root/build/probe-$arm"; file "$root/build/probe-$arm"; done; "$root/build/probe-old" /home/strata/.scratch/codex-sortable-speed/build/real-values.tsv 0 finite > "$root/build/linux-old-check.tsv"'
quiet exclusive c10-linux-four 10 'Linux VM old strict safety trusted decode on 31 profiles' -- ssh -i "$KEY" -o UserKnownHostsFile="$KNOWN" -p 18772 strata@127.0.0.1 'set -e; root=/home/strata/.scratch/codex-sortable-trusted; corpus=/home/strata/.scratch/codex-sortable-speed/build/real-values.tsv; for run in 1 2 3; do for arm in old string safety trusted; do "$root/build/probe-$arm" "$corpus" 40 finite > "$root/build/linux-c10b-$arm-$run.tsv"; done; done'
quiet light c10-vm-results 2 'Fetch Linux timing runs' -- scp -q -i "$KEY" -o UserKnownHostsFile="$KNOWN" -P 18772 'strata@127.0.0.1:/home/strata/.scratch/codex-sortable-trusted/build/linux-c10b-*.tsv' build/
/usr/local/bin/python3 -I bench/summarize_decode.py build/linux-c10b
```
