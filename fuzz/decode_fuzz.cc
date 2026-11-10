// libFuzzer target for the decoder (T4): arbitrary bytes are rejected or re-encode to themselves, and
// every conversion of an accepted value agrees with the others and truncates toward zero; the same
// holds for each half of the input split in two, whose memcmp order equals numeric order, with the
// conversions keeping it too; its first eight bytes, taken as each type, round trip.
// Build: cmake -B build-fuzz -DSORTABLE_FUZZ=ON (clang). Run: build-fuzz/sortable_fuzz DIR fuzz/corpus,
// with DIR from tools/make_corpus.py (split-prefixed seeds); fuzz/corpus holds raw encodings.
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#include "../test/exact.hh"
#include "sortable.hh"

namespace {

[[noreturn]] void fail(const char* what, std::string_view a, std::string_view b = {}) {
	std::fprintf(stderr, "sortable fuzz: %s: %s %s\n", what, exact::hex(a).c_str(), exact::hex(b).c_str());
	std::abort();
}

// Every conversion of a decoded value, as a caller reading a stored value would run them.
struct converted {
	std::optional<std::int64_t> i;
	std::optional<std::uint64_t> u;
	double d;
	std::int64_t si;
	std::uint64_t su;

	explicit converted(const sortable::number& n)
		: i(n.to_int64()), u(n.to_uint64()), d(n.to_double()), si(n.saturate_int64()), su(n.saturate_uint64()) {}
};

void check_conversions(const sortable::number& n, const converted& c, std::string_view in) {
	if (c.i && c.si != *c.i) fail("saturate_int64 differs from to_int64", in);
	if (c.u && c.su != *c.u) fail("saturate_uint64 differs from to_uint64", in);
	// An integer rounds to the nearest double, ties to even, as the language's conversion does.
	if ((c.i && c.d != static_cast<double>(*c.i)) || (c.u && c.d != static_cast<double>(*c.u))) fail("to_double not nearest", in);
	if (n.is_nan()) {
		if (!std::isnan(c.d) || c.si != 0 || c.su != 0) fail("NaN converts to a number", in);
		return;
	}
	// Truncation toward zero: between 0 and the value, inclusive.
	const exact::Key v = exact::from_number(n), zero{};
	const exact::Key si = exact::from_int64(c.si), su = exact::from_uint64(c.su);
	const bool below = exact::compare(v, zero) < 0;
	if (below ? (exact::compare(si, v) < 0 || exact::compare(si, zero) > 0) : (exact::compare(si, v) > 0 || exact::compare(si, zero) < 0))
		fail("saturate_int64 not toward zero", in);
	if (below ? c.su != 0 : exact::compare(su, v) > 0) fail("saturate_uint64 not toward zero", in);
	if (std::isnan(c.d)) fail("a number converts to NaN", in);
}

// Decodes `in`; an accepted input re-encodes to itself and converts consistently.
std::optional<sortable::number> accept(std::string_view in) {
	const auto n = sortable::decode(in);
	if (n) {
		if (exact::reencode(*n) != in) fail("accepted, not canonical", in);
		check_conversions(*n, converted(*n), in);
	}
	return n;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view in(reinterpret_cast<const char*>(data), size);
	accept(in);

	if (size >= 1) {
		const std::size_t cut = data[0] % size;
		const std::string_view a = in.substr(1, cut), b = in.substr(1 + cut);
		const auto x = accept(a), y = accept(b);
		if (x && y) {
			const int c = exact::compare(exact::from_number(*x), exact::from_number(*y));
			if (c != exact::memcmp_order(a, b)) fail("order", a, b);
			// Rounding and saturation are monotone (NaN, above +inf in the order, converts to 0 or NaN).
			if (c < 0 && !y->is_nan()) {
				const converted cx(*x), cy(*y);
				if (cx.d > cy.d || cx.si > cy.si || cx.su > cy.su) fail("conversion order", a, b);
			}
		}
	}

	if (size >= 8) {
		std::uint64_t bits;
		std::memcpy(&bits, data, 8);
		const double d = std::bit_cast<double>(bits);
		const auto i = static_cast<std::int64_t>(bits);
		const std::string ed = sortable::encode(d), ei = sortable::encode(i), eu = sortable::encode(bits);
		for (const std::string* e : {&ed, &ei, &eu}) {
			const auto back = sortable::decode(*e);
			if (!back || exact::reencode(*back) != *e) fail("round trip", *e);
		}
		if (exact::compare(exact::from_double(d), exact::from_int64(i)) != exact::memcmp_order(ed, ei)) fail("cross-type order", ed, ei);
		if (exact::compare(exact::from_uint64(bits), exact::from_int64(i)) != exact::memcmp_order(eu, ei)) fail("cross-type order", eu, ei);
	}
	return 0;
}
