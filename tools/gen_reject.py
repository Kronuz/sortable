"""Writes test/reject.txt: one "rule hex" line per input the decoder must reject (D5).

Usage: python3 -I tools/gen_reject.py > test/reject.txt
The witnesses come from model.py (`rules`); extra ones cover the other tags and both signs.
"""

import os
import sys
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model

NEG = model.NEG_INF


def enc_raw(x):
    """model.encode() for a Fraction outside the codec's domain (it asserts the integer range)."""
    from fractions import Fraction

    x = Fraction(x)
    a = abs(x)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if Fraction(2) ** e > a:
        e -= 1
    f = a / Fraction(2) ** e - 1
    if x > 0:
        tag, field = model._tag(e)
        return bytes([tag]) + field + model._tail(f)
    u = e if f == 0 else e + 1
    tag, field = model._tag(u)
    return bytes([0xFF - tag]) + bytes(0xFF - c for c in field) + (b"" if f == 0 else model._tail(1 - f))


def lines():
    w = model._witness
    yield "D5.1 empty", b""
    yield "D5.1 tag-00", b"\x00"
    yield "D5.1 tag-7f", b"\x7f"
    yield "D5.1 tag-00-tail", b"\x00\x01"
    yield "D5.1 tag-7f-tail", b"\x7f\x01"
    yield "D5.2 zero-tail", w("special_with_tail")
    yield "D5.2 nan-tail", b"\xff\x00"
    yield "D5.3 low-escape-no-field", b"\x81"
    yield "D5.3 low-escape-short-field", w("short_field")
    yield "D5.3 high-escape-no-field", b"\xfe"
    yield "D5.3 high-escape-short-field", b"\xfe\x00"
    yield "D5.3 neg-high-escape-no-field", b"\x01"
    yield "D5.3 neg-low-escape-short-field", b"\x7e\xff"
    yield "D5.4 low-exponent-fits-direct", w("escape_exponent_range")
    yield "D5.4 low-exponent-above-direct", b"\x81\xff\xff"
    yield "D5.4 high-exponent-1024", b"\xfe\x03\xc0"
    yield "D5.4 high-exponent-max-field", b"\xfe\xff\xfe"
    yield "D5.4 neg-high-exponent-1025", b"\x01\xfc\x3e"
    yield "D5.4 neg-high-exponent-min-field", b"\x01\x00\x01"
    yield "D5.4 neg-low-exponent-fits-direct", b"\x7e\xfc\x09"
    yield "D5.5 pos-inf-tail", w("inf_with_tail")
    yield "D5.5 neg-inf-tail", NEG + b"\x01"
    yield "D5.6 tail-9-bytes", w("tail_too_long")
    yield "D5.6 neg-tail-9-bytes", b"\x41" + b"\x80" + b"\x00" * 7 + b"\x01"
    yield "D5.7 last-byte-zero", w("tail_last_byte_zero")
    yield "D5.7 neg-last-byte-zero", b"\x41\x80\x00"
    yield "D5.7 only-zero-tail", b"\xbe\x00"
    yield "D5.8 tail-64-bits", w("tail_64_bits")
    yield "D5.8 neg-tail-64-bits", b"\x41" + b"\x80" + b"\x00" * 6 + b"\x01"
    yield "D5.9 not-double-or-integer", w("not_double_or_integer")
    yield "D5.9 neg-not-double-or-integer", b"\x41" + b"\x00" * 7 + b"\x10"
    yield "D5.9 subnormal-needs-two-bits", b"\x81\x00\x01\x40"  # 2^-1073 * 1.25
    yield "D5.9 neg-subnormal-needs-extra-bit", b"\x7e\xff\xfe\xc0"  # -(2^-1074 * 1.5) wrapped
    yield "D5.9 neg-integer-below-int64-min", enc_raw(-(2**63) - 2**10)  # -(2^63 + 2^10): not a double
    yield "D5.9 integer-above-uint64-max", enc_raw(2**64 + 2**11)  # 2^64 + 2^11: not a double
    yield "D5.9 half-integer-above-2^53", enc_raw(Fraction(2**61 + 1, 2))  # 2^60 + 1/2: neither
    yield "D5.9 53-bit-fraction", enc_raw(Fraction(1, 2**100) * (1 + Fraction(1, 2**53)))
    yield "D5.9 2^1024-negative", bytes([0xFF - 0xFE]) + (0xFFFF - 960).to_bytes(2, "big")  # -2^1024
    yield "D5.9 below-least-subnormal", b"\x81\x00\x00\x80"  # 2^-1074 * 1.5


for name, b in lines():
    x = model.decode(b)
    assert x is None, (name, b.hex(), x)
    print(name, b.hex() or "-")
