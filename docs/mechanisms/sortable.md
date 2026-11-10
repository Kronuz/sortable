> Status: CURRENT (2026-11-09) how `sortable.hh` encodes a number. The bytes are fixed by `tools/model.py`.

# How sortable encodes a number

Every number becomes a short byte string whose `memcmp` order is the numeric order. An `int64`, a
`uint64` and a `double` that hold the same number give the same bytes, so a column that mixes the three
needs no conversion to sort or range over it. The order is

```
-inf < negatives < 0 < positives < +inf < NaN
```

## The shape

A finite nonzero value is |v| = 2^E x (1 + f), with E its binary exponent and f a fraction of at most 63
bits (every `uint64` and every `double` fits; a subnormal `double` is normalised first). The encoding is
a tag byte, an escape field only for the extreme tags, and a tail.

```
 tag        escape field        tail
 1 byte     2 bytes (rare)      0..8 bytes: f x 2^(8n), big-endian, last byte nonzero

 0x80 zero       0xFF NaN       0x00 and 0x7F are never written
 0x82..0xFD      positive, E = -60..63 (the tag is 0x82 + E + 60); no escape field
 0x81            positive, E < -60: field E + 1074
 0xFE            positive, E >= 64: field E - 64; +inf is field 0xFFFF
 negatives       the mirror: 0xFF - tag, field 0xFFFF - field
```

One tag per binary exponent covers E = -60 to 63, so every integer and every `double` down to 2^-60 takes
a single tag byte: 5 is `c040`, 1 is `be`, a score in [0, 1) is a tag and 7 bytes. The longest encoding
is 10 bytes (a `double` outside the window: tag, 2-byte field, 7 tail bytes); an integer takes at most 9.

## Why memcmp is the numeric order

Across tags the exponent decides, and tags rise with the exponent; the escape fields are big-endian, and
the escapes bound the direct tags on both sides, so the order holds there too. Inside one tag the order is
the tail's: a shorter tail is a longer one with zeros after it, and `memcmp` puts a prefix first, so the
fraction 0 (an exact power of two, the empty tail) sorts first.

A negative reverses all of it without a terminator. Its magnitude lies in (2^(U-1), 2^U], and it takes
the tag of U with the tail 1 - f: a larger magnitude has a smaller 1 - f, and the lowest set bit of 1 - f
is the lowest set bit of f, so the tail keeps its length and its nonzero last byte. -(2^U), the most
negative value of its tag, has the empty tail and sorts first. The old Xapian codec complemented a
trailing-zero-stripped tail instead, so a shorter negative was a prefix of a longer one and sorted first:
-(2^30) sorted before -(2^30) - 1.

The encoding is not prefix-free. A caller that concatenates encodings frames them (escape `0x00`, end each
with a terminator) or stores one number per value.

## Special values

1. -0.0 is 0: `80`. The sign of a negative zero does not round trip.
2. Every NaN is the single byte `ff`, above +inf, with payload and sign dropped. The codec never fails on
   input.
3. +inf is `feffff` and -inf is `010000`: the escape tag with the extreme field.
4. `max_finite` and `min_finite` are the encodings of DBL_MAX and -DBL_MAX: a missing sort value that
   sorts after (or before) every finite number and below +inf (above -inf).

## The decoder

`decode` accepts exactly the outputs of `encode` and nothing else. It reads only within its input,
allocates nothing and never throws. It rejects:

1. The empty input, and the tags never written (`0x00`, `0x7F`).
2. Zero or NaN followed by any byte.
3. An escape tag without its 2-byte field.
4. An escape exponent that fits a direct tag, or beyond 1023 (1024 for a negative, whose exponent is one
   above its magnitude's).
5. +inf or -inf followed by any byte.
6. A tail of more than 8 bytes.
7. A tail whose last byte is 0.
8. A tail of 8 bytes with its lowest bit set (64 fraction bits).
9. A value that is neither a `double` nor an integer in [-2^63, 2^64).

Rules 6 and 8 are implied by rule 9; they come first so the decoder bounds its reads. The result is a
`number`: sign, exponent and a 64-bit significand, or a special, with `to_int64`, `to_uint64`,
`to_double` (nearest-even above 2^53) and the saturating `saturate_int64` and `saturate_uint64`.
Everything is integer arithmetic and `std::bit_cast`, so the bytes are the same on every platform.

Every conversion is defined for every `number`, with no undefined behavior: every shift count is bounded
and `-1022 - exponent` cannot overflow an `int`. `decode` only produces numbers whose exponent lies in
[-1074, 1023] (or [0, 63] for an integer); a `number` built by hand with another exponent or significand
still converts, but to a value that no `decode` produces.

## What fixes the bytes

`tools/model.py` is the executable reference. `test/vectors.txt` (113,491 lines) is its output, and
`test/reject.txt` its witnesses for each reject rule; the C++ tests check both on every platform, and
`tools/mutants.py` shows each rule is needed by dropping it.

## How the decoder is proven

The decoder reads bytes from users' disks, so its proof does not rest on sampled inputs alone:

1. `sortable_test` decodes every input of up to 3 bytes; `sortable_property` checks round trip, order and
   cross-type equality at every boundary and over random values against `test/exact.hh`.
2. `sortable_exhaustive_float` (`-DSORTABLE_EXHAUSTIVE=ON`, minutes, not in CI) covers all 2^32 `float`
   bit patterns: each non-NaN round trips bit for bit in value order with strictly increasing bytes, and
   every NaN of either sign is `ff`. It counts the patterns and fails unless the count is 2^32.
3. `fuzz/decode_fuzz.cc` (libFuzzer, ASan and UBSan, aborting on the first report) checks, for the whole
   input and for each half of it split in two, that an accepted input re-encodes to itself and that its
   conversions agree (a saturated value equals the exact one when that exists, an integer's `to_double`
   equals the language's own conversion, saturation lies between 0 and the value); that the two halves
   compare by `memcmp` as by value and their conversions keep that order; and that the first 8 bytes,
   taken as each type, round trip. The seeds are `tools/make_corpus.py`'s (each behind a split byte) and
   `fuzz/corpus` (190 raw encodings, so that whole inputs are accepted from the first run).
4. `tools/mutants.py` drops one rule at a time and needs a test to fail for each.

## The command-line driver

`sortable_cli encode` and `sortable_cli decode` run the codec over stdin lines, one answer per line,
for tests in other languages. `encode` reads `i DECIMAL` (an integer in [-2^63, 2^64)) or `f HEX16`
(binary64 bits) and writes the encoding's hex. `decode` reads an input's hex (an empty line is the empty
input) and writes `reject` or the value in `encode`'s input form: `i` for an integer in [-2^63, 2^64),
`f` for anything else, with every NaN as `f 7ff8000000000000`. So a decoded answer fed back to `encode`
gives the input. The readers are lenient in three ways, all accepted: an `i` number may have leading zeros
(`i 007`) or be `-0` (the integer 0), and hex digits, in `f HEX16` and in `decode` lines, may be upper or
lower case. A malformed line is an error (exit 2), never an answer. `tools/check_cli.py`, the
`sortable_cli` test, drives it against `tools/model.py`: every vector, 100,000 seeded random values, every
0 to 2 byte string, every 3-byte string starting with an escape tag, the reject witnesses and 200,000
seeded mutations, then each accepted answer back through `encode`.
