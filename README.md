# sortable

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/sortable/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/sortable/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)

Exact, memcmp-ordered number codec for C++20: int64, uint64 and double share one byte order in 1
to 10 bytes. A tag byte for the binary exponent leads the big-endian fraction, so byte order is
numeric order, and 5, `5u` and 5.0 encode to the same bytes. Reach for it for sort keys, range
scans and index values that compare with `memcmp` and never convert.

## ✨ Highlights

- **Exact.** Every `int64` and `uint64` round trips, so 2147483649 sorts before 2147483650 and
  -(2^30) sorts after -(2^30) - 1. Codecs that squeeze a number through a float or a stripped
  tail get those wrong.
- **One order for all types.** An integer column and a floating column that meet in one index
  compare with `memcmp`: no tag byte, no conversion.
- **Short.** A single tag byte covers every exponent from 2^-60 to 2^63; 1 is one byte, 5 is two,
  a Unix timestamp in seconds is four. Never more than 10 bytes.
- **A strict decoder.** `decode` takes any bytes, reads only inside them, allocates nothing and
  never throws; it accepts exactly what `encode` writes. A libFuzzer target checks it.
- **Total.** -0.0 is 0, every NaN is one byte that sorts last, and the infinities have their own
  encodings; the encoder never fails.
- **Same bytes everywhere.** Only integer arithmetic and `std::bit_cast`: no `long double`, no
  platform behavior. A Python reference model (`tools/model.py`) writes the test vectors the C++
  tests check on every platform.

## 🚀 Quick start

```cpp
#include "sortable.hh"

#include <cassert>
#include <cstdint>
#include <string>

int main() {
	// The same number in three types is the same bytes, and bytes order like numbers.
	assert(sortable::encode(5) == sortable::encode(5u));
	assert(sortable::encode(5u) == sortable::encode(5.0));
	assert(sortable::encode(std::int64_t{2147483649}) < sortable::encode(std::int64_t{2147483650}));
	assert(sortable::encode(-2.5) < sortable::encode(std::int64_t{-2}));

	char buf[sortable::max_size];                         // no allocation
	std::size_t n = sortable::encode(0.01, buf);          // 8 bytes: b747ae147ae147b0

	auto v = sortable::decode(std::string_view(buf, n));  // nullopt unless buf is exactly an encoding
	assert(v && v->to_double() == 0.01 && !v->to_int64());
	assert(v->saturate_int64() == 0);                     // truncates toward zero, clamps, NaN is 0
}
```

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; no other dependencies. CI builds and tests it
on Linux (x86_64 with GCC and Clang, arm64), macOS (arm64, x86_64) and FreeBSD, plus ASan + UBSan
and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(sortable
  GIT_REPOSITORY https://github.com/Kronuz/sortable.git
  GIT_TAG        1aa2f9837671ae5c3007d9c41cf9e07b4126072a)  # or main to track the tip
FetchContent_MakeAvailable(sortable)

target_link_libraries(my_app PRIVATE sortable::sortable)
```

```cpp
#include "sortable.hh"
```

Or copy `sortable.hh` onto your include path.

## 🎯 When to use it

Sort keys, value slots and index terms where integers and floating-point numbers share an order,
compared with `memcmp`, and range bounds that must be exact for 64-bit integers.

Not for:

- Columns of one fixed 64-bit type that never mix: an 8-byte big-endian map with the sign bit
  flipped is fixed-width and simpler.
- Concatenating several numbers without framing: the encoding is not prefix-free, so frame each
  one (escape `0x00` and end with a terminator).

## 📚 Documentation

- [docs/mechanisms/sortable.md](docs/mechanisms/sortable.md): the byte layout, why the order
  holds, special values and the decoder's nine reject rules.
- [AGENTS.md](AGENTS.md): repository map, invariants and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries): the catalog of the family.

## 🛠️ Building and testing

```sh
cmake -B build && cmake --build build && ctest --test-dir build
cmake -B build -DSORTABLE_EXHAUSTIVE=ON && cmake --build build && ctest --test-dir build
cmake -B build -DSORTABLE_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++ && cmake --build build
python3 -I tools/mutants.py
```

The tests check the format's examples, the model's 113,491 vectors, a witness for each reject
rule, every input of up to three bytes (and four behind each escape tag), and round trip, order
and cross-type equality over every `int64` near zero, every power of two with its neighbors,
every `double` exponent and random values of each type. `SORTABLE_EXHAUSTIVE` adds a pass over all
2^32 `float` values and `SORTABLE_FUZZ` the libFuzzer target; `tools/mutants.py` drops each
reject rule in turn and shows a test fail.

## 🤝 Contributing

Issues and pull requests are welcome. Read [AGENTS.md](AGENTS.md) and the catalog's
[rules of engagement](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md#rules-of-engagement)
first; CI must pass on every platform.

## 📄 License

MIT. See [LICENSE](LICENSE).
