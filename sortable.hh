// sortable: an exact, memcmp-ordered number codec for int64, uint64 and double.
//
// MIT License. Copyright (c) 2026 Germán Méndez Bravo. See LICENSE.
//
// Every number encodes to bytes whose memcmp order is its numeric order, across types: 5, 5u and 5.0 are
// one encoding. The order is -inf < negatives < 0 < positives < +inf < NaN. The format is described in
// docs/mechanisms/sortable.md and fixed by tools/model.py.

#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace sortable {

// The longest encoding: a tag, a 2-byte exponent and 7 bytes of a double's fraction.
inline constexpr std::size_t max_size = 10;

template <class T>
inline constexpr bool is_character_v = std::same_as<T, char> || std::same_as<T, wchar_t> ||
	std::same_as<T, char8_t> || std::same_as<T, char16_t> || std::same_as<T, char32_t>;

// Every standard integer type up to 64 bits (not bool or the character types), float and double.
template <class T>
concept encodable =
	(std::integral<T> && !std::same_as<T, bool> && !is_character_v<T> && sizeof(T) <= 8) ||
	std::same_as<T, float> || std::same_as<T, double>;

// A decoded value: a special, or sign, binary exponent and a 64-bit significand 1.f (top bit set).
// It is a double or an integer in [-2^63, 2^64); the typed conversions say which.
struct number {
	enum class kind : std::uint8_t { zero, finite, pos_inf, neg_inf, nan };

	kind type = kind::zero;
	bool negative = false;
	int exponent = 0;               // finite: |v| = 2^exponent * significand / 2^63
	std::uint64_t significand = 0;  // finite: bit 63 is set

	constexpr bool is_nan() const noexcept { return type == kind::nan; }
	constexpr bool is_inf() const noexcept { return type == kind::pos_inf || type == kind::neg_inf; }
	constexpr bool is_zero() const noexcept { return type == kind::zero; }

	// The value as an int64 or uint64 when it is one exactly.
	constexpr std::optional<std::int64_t> to_int64() const noexcept {
		if (type == kind::zero) return 0;
		std::uint64_t mag;
		if (!integer_magnitude(mag)) return std::nullopt;
		if (negative) {
			if (mag > std::uint64_t{1} << 63) return std::nullopt;
			return static_cast<std::int64_t>(std::uint64_t{0} - mag);
		}
		if (mag > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return std::nullopt;
		return static_cast<std::int64_t>(mag);
	}

	constexpr std::optional<std::uint64_t> to_uint64() const noexcept {
		if (type == kind::zero) return 0;
		std::uint64_t mag;
		if (negative || !integer_magnitude(mag)) return std::nullopt;
		return mag;
	}

	// Truncate toward zero and saturate; NaN is 0.
	constexpr std::int64_t saturate_int64() const noexcept {
		constexpr auto lo = std::numeric_limits<std::int64_t>::min();
		constexpr auto hi = std::numeric_limits<std::int64_t>::max();
		switch (type) {
			case kind::zero:
			case kind::nan: return 0;
			case kind::pos_inf: return hi;
			case kind::neg_inf: return lo;
			case kind::finite: break;
		}
		if (exponent < 0) return 0;
		if (exponent > 63) return negative ? lo : hi;
		const std::uint64_t mag = significand >> (63 - exponent);
		if (negative) return mag > std::uint64_t{1} << 63 ? lo : static_cast<std::int64_t>(std::uint64_t{0} - mag);
		return mag > static_cast<std::uint64_t>(hi) ? hi : static_cast<std::int64_t>(mag);
	}

	// Truncate toward zero and saturate; NaN and anything not above 0 is 0.
	constexpr std::uint64_t saturate_uint64() const noexcept {
		switch (type) {
			case kind::zero:
			case kind::nan:
			case kind::neg_inf: return 0;
			case kind::pos_inf: return std::numeric_limits<std::uint64_t>::max();
			case kind::finite: break;
		}
		if (negative || exponent < 0) return 0;
		if (exponent > 63) return std::numeric_limits<std::uint64_t>::max();
		return significand >> (63 - exponent);
	}

	// Exact for every double; an integer above 2^53 rounds to nearest even. NaN is the canonical quiet NaN.
	constexpr double to_double() const noexcept {
		constexpr std::uint64_t sign_bit = std::uint64_t{1} << 63;
		const std::uint64_t sign = negative ? sign_bit : 0;
		switch (type) {
			case kind::zero: return std::bit_cast<double>(sign);
			case kind::pos_inf: return std::numeric_limits<double>::infinity();
			case kind::neg_inf: return -std::numeric_limits<double>::infinity();
			case kind::nan: return std::numeric_limits<double>::quiet_NaN();
			case kind::finite: break;
		}
		int e = exponent;
		if (e > 1023) return std::bit_cast<double>(sign | (std::uint64_t{0x7FF} << 52));
		// Significand bits kept: 53, fewer in the subnormal range.
		const int keep = e < -1022 ? 53 - (-1022 - e) : 53;
		if (keep < 0) return std::bit_cast<double>(sign);
		std::uint64_t q;
		if (keep == 0) {
			// Between 0 and the least subnormal: halfway (2^-1075 exactly) rounds to even, 0.
			q = significand > sign_bit ? 1 : 0;
		} else {
			const int drop = 64 - keep;
			q = significand >> drop;
			const std::uint64_t rem = significand & ((std::uint64_t{1} << drop) - 1);
			const std::uint64_t half = std::uint64_t{1} << (drop - 1);
			if (rem > half || (rem == half && (q & 1))) ++q;
		}
		if (e < -1022) return std::bit_cast<double>(sign | q);  // a carry into bit 52 is the least normal
		if (q == std::uint64_t{1} << 53) {
			q >>= 1;
			++e;
			if (e > 1023) return std::bit_cast<double>(sign | (std::uint64_t{0x7FF} << 52));
		}
		return std::bit_cast<double>(sign | (static_cast<std::uint64_t>(e + 1023) << 52) | (q & ((std::uint64_t{1} << 52) - 1)));
	}

	friend constexpr bool operator==(const number&, const number&) noexcept = default;

   private:
	// |v| as an integer, when the value is one that fits 64 bits.
	constexpr bool integer_magnitude(std::uint64_t& mag) const noexcept {
		if (type != kind::finite || exponent < 0 || exponent > 63) return false;
		const int shift = 63 - exponent;
		if ((significand & ((std::uint64_t{1} << shift) - 1)) != 0) return false;
		mag = significand >> shift;
		return true;
	}
};

namespace detail {

// Direct tags cover exponents lo_exp..hi_exp: positive tag 0x82 + (E + 60), negatives mirrored.
inline constexpr int lo_exp = -60;
inline constexpr int hi_exp = 63;
inline constexpr int min_exp = -1074;  // the least subnormal's exponent: the low escape field is E - min_exp
inline constexpr std::uint8_t tag_zero = 0x80, tag_nan = 0xFF, tag_esc_low = 0x81, tag_esc_high = 0xFE;

// Writes a finite nonzero value |v| = 2^e (1 + f), f = frac / 2^64, with its sign. Returns the length.
constexpr std::size_t put(bool neg, int e, std::uint64_t frac, char* out) noexcept {
	int u = e;
	std::uint64_t tail = frac;
	if (neg && frac != 0) {
		u = e + 1;                 // D3: a negative takes the tag of the power of two at or above it
		tail = std::uint64_t{0} - frac;  // D3: and the tail 1 - f
	}
	std::uint8_t tag;
	std::uint32_t field = 0;
	bool escape = true;
	if (u >= lo_exp && u <= hi_exp) {
		tag = static_cast<std::uint8_t>(0x82 + (u - lo_exp));
		escape = false;
	} else if (u < lo_exp) {
		tag = tag_esc_low;
		field = static_cast<std::uint32_t>(u - min_exp);
	} else {
		tag = tag_esc_high;
		field = static_cast<std::uint32_t>(u - (hi_exp + 1));
	}
	if (neg) {
		tag = static_cast<std::uint8_t>(0xFF - tag);  // D3: the mirrored tag
		field = 0xFFFF - field;                       // D3: the complemented field
	}
	std::size_t n = 0;
	out[n++] = static_cast<char>(tag);
	if (escape) {
		out[n++] = static_cast<char>(field >> 8);
		out[n++] = static_cast<char>(field & 0xFF);
	}
	if (tail != 0) {
		// The bytes of the left-aligned tail up to its last nonzero byte.
		const int len = 8 - (std::countr_zero(tail) >> 3);
		for (int i = 0; i < len; ++i) out[n++] = static_cast<char>(tail >> (56 - 8 * i));
	}
	return n;
}

constexpr std::size_t put_double(double v, char* out) noexcept {
	const auto bits = std::bit_cast<std::uint64_t>(v);
	const bool neg = (bits >> 63) != 0;
	const int biased = static_cast<int>((bits >> 52) & 0x7FF);
	const std::uint64_t m = bits & ((std::uint64_t{1} << 52) - 1);
	if (biased == 0x7FF) {
		if (m != 0) {
			out[0] = static_cast<char>(tag_nan);
			return 1;
		}
		out[0] = static_cast<char>(neg ? 0xFF - tag_esc_high : tag_esc_high);
		out[1] = static_cast<char>(neg ? 0x00 : 0xFF);
		out[2] = static_cast<char>(neg ? 0x00 : 0xFF);
		return 3;
	}
	if (biased == 0) {
		if (m == 0) {
			out[0] = static_cast<char>(tag_zero);  // -0.0 is 0
			return 1;
		}
		const int lz = std::countl_zero(m);  // a subnormal normalises
		return put(neg, (63 - lz) + min_exp, (m << lz) << 1, out);
	}
	return put(neg, biased - 1023, m << 12, out);
}

constexpr std::size_t put_integer(bool neg, std::uint64_t mag, char* out) noexcept {
	if (mag == 0) {
		out[0] = static_cast<char>(tag_zero);
		return 1;
	}
	const int lz = std::countl_zero(mag);
	return put(neg, 63 - lz, (mag << lz) << 1, out);
}

template <class T>
constexpr std::size_t encode(T v, char* out) noexcept {
	if constexpr (std::is_floating_point_v<T>) {
		return put_double(static_cast<double>(v), out);
	} else if constexpr (std::is_signed_v<T>) {
		const auto x = static_cast<std::int64_t>(v);
		return put_integer(x < 0, x < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(x) : static_cast<std::uint64_t>(x), out);
	} else {
		return put_integer(false, static_cast<std::uint64_t>(v), out);
	}
}

// Bytes of an encoding, for the constants below.
struct fixed {
	char bytes[max_size] = {};
	std::size_t size = 0;
};

constexpr fixed make_fixed(double v) noexcept {
	fixed f;
	f.size = detail::encode(v, f.bytes);
	return f;
}

inline constexpr fixed max_finite_storage = make_fixed(std::numeric_limits<double>::max());
inline constexpr fixed min_finite_storage = make_fixed(std::numeric_limits<double>::lowest());

}  // namespace detail

// Writes the encoding of v to out, which holds at least max_size bytes. Returns the bytes written.
// Total: any input encodes. -0.0 is 0; every NaN is the single byte 0xFF.
template <encodable T>
constexpr std::size_t encode(T v, char* out) noexcept {
	return detail::encode(v, out);
}

template <encodable T>
std::string encode(T v) {
	char buf[max_size];
	return std::string(buf, encode(v, buf));
}

// The encodings of DBL_MAX and -DBL_MAX: a missing sort value that sorts after (or before) every finite
// number and below +inf (or above -inf).
inline constexpr std::string_view max_finite{detail::max_finite_storage.bytes, detail::max_finite_storage.size};
inline constexpr std::string_view min_finite{detail::min_finite_storage.bytes, detail::min_finite_storage.size};

// Strict decode: the value when `bytes` is exactly what encode() writes for it, else nothing. Reads only
// within `bytes`, allocates nothing and never throws. Rules D1..D9 of the format (see the mechanism note).
constexpr std::optional<number> decode(std::string_view bytes) noexcept {
	using namespace detail;
	const std::size_t size = bytes.size();
	const char* p = bytes.data();
	auto at = [p](std::size_t i) { return static_cast<std::uint8_t>(p[i]); };

	if (size == 0) return std::nullopt;                      // D5.1
	if (at(0) == 0x00 || at(0) == 0x7F) return std::nullopt;  // D5.1: tags never written
	if (at(0) == tag_zero || at(0) == tag_nan) {
		if (size > 1) return std::nullopt;  // D5.2
		number r;
		r.type = at(0) == tag_zero ? number::kind::zero : number::kind::nan;
		return r;
	}
	const bool neg = at(0) < tag_zero;
	const int t = neg ? 0xFF - at(0) : at(0);
	std::size_t i = 1;
	int e;
	if (t >= 0x82 && t <= 0xFD) {
		e = lo_exp + (t - 0x82);
	} else {
		if (size < 3) return std::nullopt;  // D5.3
		int raw = (at(1) << 8) | at(2);
		if (neg) raw = 0xFFFF - raw;
		i = 3;
		if (t == tag_esc_high && raw == 0xFFFF) {
			if (size > 3) return std::nullopt;  // D5.5
			number r;
			r.type = neg ? number::kind::neg_inf : number::kind::pos_inf;
			r.negative = neg;
			return r;
		}
		if (t == tag_esc_low) {
			e = raw + min_exp;
			if (e >= lo_exp) return std::nullopt;  // D5.4: it fits a direct tag
		} else {
			e = raw + hi_exp + 1;
			if (e > (neg ? 1024 : 1023)) return std::nullopt;  // D5.4: beyond a double
		}
	}
	const std::size_t len = size - i;
	if (len > 8) return std::nullopt;                                // D5.6
	if (len != 0 && at(size - 1) == 0) return std::nullopt;          // D5.7
	if (len == 8 && (at(size - 1) & 1) != 0) return std::nullopt;    // D5.8
	std::uint64_t tail = 0;
	for (std::size_t k = 0; k < len; ++k) tail |= static_cast<std::uint64_t>(at(i + k)) << (56 - 8 * k);
	std::uint64_t frac = tail;
	if (neg && len != 0) {
		e -= 1;                            // D3: the magnitude is in (2^(U-1), 2^U)
		frac = std::uint64_t{0} - tail;    // D3: the tail is 1 - f
	}
	// Fraction bits: the position of the lowest set bit of f.
	const int k = frac == 0 ? 0 : 64 - std::countr_zero(frac);
	bool is_double = false;
	if (e >= min_exp && e <= 1023) is_double = e >= -1022 ? k <= 52 : k <= e - min_exp;
	bool is_integer = false;
	if (e >= 0 && k <= e) is_integer = neg ? (e <= 62 || (e == 63 && frac == 0)) : e <= 63;
	if (!is_double && !is_integer) return std::nullopt;  // D5.9
	number r;
	r.type = number::kind::finite;
	r.negative = neg;
	r.exponent = e;
	r.significand = (std::uint64_t{1} << 63) | (frac >> 1);
	return r;
}

}  // namespace sortable
