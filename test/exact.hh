// An exact, codec-independent comparator for the tests and the fuzz target: a value of any type as
// (rank, sign, exponent, significand), ordered without the codec. Doubles go through frexp, integers
// through a shift loop.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "sortable.hh"

namespace exact {

struct Key {
	std::int8_t rank = 1;      // 0 -inf, 1 finite, 2 +inf, 3 NaN
	std::int8_t sign = 0;      // finite: -1, 0, 1
	std::int16_t exponent = 0; // |v| = 2^exponent * significand / 2^63
	std::uint64_t significand = 0;

	constexpr Key() = default;
	constexpr Key(int r, int s, int e, std::uint64_t m)
		: rank(static_cast<std::int8_t>(r)), sign(static_cast<std::int8_t>(s)), exponent(static_cast<std::int16_t>(e)), significand(m) {}
};

inline int compare(const Key& a, const Key& b) {
	if (a.rank != b.rank) return a.rank < b.rank ? -1 : 1;
	if (a.rank != 1) return 0;
	if (a.sign != b.sign) return a.sign < b.sign ? -1 : 1;
	if (a.sign == 0) return 0;
	int c = 0;
	if (a.exponent != b.exponent) c = a.exponent < b.exponent ? -1 : 1;
	else if (a.significand != b.significand) c = a.significand < b.significand ? -1 : 1;
	return a.sign > 0 ? c : -c;
}

inline Key from_double(double d) {
	Key k;
	if (std::isnan(d)) return Key{3, 0, 0, 0};
	if (std::isinf(d)) return Key{d < 0 ? 0 : 2, 0, 0, 0};
	if (d == 0) return k;
	int e = 0;
	const double m = std::frexp(std::fabs(d), &e);  // m in [0.5, 1)
	k.sign = static_cast<std::int8_t>(d < 0 ? -1 : 1);
	k.exponent = static_cast<std::int16_t>(e - 1);
	k.significand = static_cast<std::uint64_t>(std::ldexp(m, 64));  // exact: m has at most 53 bits
	return k;
}

inline Key from_magnitude(bool negative, std::uint64_t mag) {
	Key k;
	if (mag == 0) return k;
	k.sign = static_cast<std::int8_t>(negative ? -1 : 1);
	k.exponent = 63;
	while ((mag >> 63) == 0) {
		mag <<= 1;
		k.exponent = static_cast<std::int16_t>(k.exponent - 1);
	}
	k.significand = mag;
	return k;
}

inline Key from_int64(std::int64_t v) {
	return from_magnitude(v < 0, v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v));
}

inline Key from_uint64(std::uint64_t v) { return from_magnitude(false, v); }

inline Key from_number(const sortable::number& n) {
	using kind = sortable::number::kind;
	switch (n.type) {
		case kind::zero: return Key{};
		case kind::pos_inf: return Key{2, 0, 0, 0};
		case kind::neg_inf: return Key{0, 0, 0, 0};
		case kind::nan: return Key{3, 0, 0, 0};
		case kind::finite: break;
	}
	return Key{1, n.negative ? -1 : 1, n.exponent, n.significand};
}

inline int memcmp_order(std::string_view a, std::string_view b) {
	const std::size_t n = a.size() < b.size() ? a.size() : b.size();
	for (std::size_t i = 0; i < n; ++i) {
		const auto x = static_cast<unsigned char>(a[i]), y = static_cast<unsigned char>(b[i]);
		if (x != y) return x < y ? -1 : 1;
	}
	return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

inline std::string hex(std::string_view s) {
	static const char* digits = "0123456789abcdef";
	std::string r;
	for (unsigned char c : s) {
		r += digits[c >> 4];
		r += digits[c & 15];
	}
	return r.empty() ? "-" : r;
}

// The canonical bytes of a decoded value: an integer when it is one, else a double.
inline std::string reencode(const sortable::number& n) {
	if (auto i = n.to_int64()) return sortable::encode(*i);
	if (auto u = n.to_uint64()) return sortable::encode(*u);
	return sortable::encode(n.to_double());
}

}  // namespace exact
