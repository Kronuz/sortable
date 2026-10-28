// Property tests (T1, T2): round trip, order and cross-type equality, against an exact comparator that
// does not use the codec. Exhaustive at the boundaries, random over the full domain of each type.
//
//   sortable_property [random-per-type]      SORTABLE_SEED=n SORTABLE_RANDOM=n
#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "check.hh"
#include "exact.hh"
#include "sortable.hh"

namespace {

using exact::Key;

enum Kind : std::uint8_t { I64, U64, F64, F32 };

struct Item {
	Key key;
	std::uint64_t raw;  // the value's bits
	Kind kind;
};

std::string bytes_of(const Item& it) {
	switch (it.kind) {
		case I64: return sortable::encode(static_cast<std::int64_t>(it.raw));
		case U64: return sortable::encode(it.raw);
		case F64: return sortable::encode(std::bit_cast<double>(it.raw));
		case F32: return sortable::encode(std::bit_cast<float>(static_cast<std::uint32_t>(it.raw)));
	}
	return {};
}

std::string describe(const Item& it) {
	char b[64];
	switch (it.kind) {
		case I64: std::snprintf(b, sizeof b, "i64 %lld", static_cast<long long>(it.raw)); break;
		case U64: std::snprintf(b, sizeof b, "u64 %llu", static_cast<unsigned long long>(it.raw)); break;
		case F64: std::snprintf(b, sizeof b, "f64 %.17g (%016llx)", std::bit_cast<double>(it.raw), static_cast<unsigned long long>(it.raw)); break;
		case F32: std::snprintf(b, sizeof b, "f32 %.9g", static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(it.raw)))); break;
	}
	return b;
}

// T1: the value decodes back exactly (bit for bit, except -0.0 and NaN), canonically, in bounds.
void round_trip(const Item& it, const std::string& enc) {
	CHECK_MSG(!enc.empty() && enc.size() <= sortable::max_size && static_cast<unsigned char>(enc[0]) != 0x00, "%s: %s", describe(it).c_str(), exact::hex(enc).c_str());
	if (it.kind == I64 || it.kind == U64) CHECK_MSG(enc.size() <= 9, "%s is %zu bytes", describe(it).c_str(), enc.size());
	auto n = sortable::decode(enc);
	CHECK_MSG(n.has_value(), "%s: %s does not decode", describe(it).c_str(), exact::hex(enc).c_str());
	if (!n) return;
	CHECK_MSG(exact::compare(exact::from_number(*n), it.key) == 0, "%s decodes to another number", describe(it).c_str());
	CHECK_MSG(exact::reencode(*n) == enc, "%s: not canonical", describe(it).c_str());
	switch (it.kind) {
		case I64: CHECK_MSG(n->to_int64() == static_cast<std::int64_t>(it.raw), "%s", describe(it).c_str()); break;
		case U64: CHECK_MSG(n->to_uint64() == it.raw, "%s", describe(it).c_str()); break;
		case F64: {
			const double d = std::bit_cast<double>(it.raw);
			const double r = n->to_double();
			if (std::isnan(d)) CHECK(std::isnan(r));
			else if (d == 0) CHECK(r == 0 && !std::signbit(r));
			else CHECK_MSG(std::bit_cast<std::uint64_t>(r) == it.raw, "%s", describe(it).c_str());
			break;
		}
		case F32: {
			const double d = static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(it.raw)));
			CHECK_MSG(n->to_double() == d || std::isnan(d), "%s", describe(it).c_str());
			break;
		}
	}
}

// T2: a sorted set's adjacent pairs agree with memcmp; equal numbers have equal bytes.
std::size_t g_checked = 0, g_pairs = 0;

void check_pair(const Item& a, const Item& b, const std::string& ea, const std::string& eb) {
	++g_pairs;
	const int c = exact::compare(a.key, b.key);
	const int m = exact::memcmp_order(ea, eb);
	CHECK_MSG(c == m, "%s (%s) against %s (%s): exact %d, memcmp %d", describe(a).c_str(), exact::hex(ea).c_str(), describe(b).c_str(), exact::hex(eb).c_str(), c, m);
}

void check_set(std::vector<Item>& items) {
	std::vector<std::string> encs;
	encs.reserve(items.size());
	for (const auto& it : items) {
		encs.push_back(bytes_of(it));
		round_trip(it, encs.back());
	}
	g_checked += items.size();
	std::vector<std::size_t> order(items.size());
	for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
	std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) { return exact::compare(items[x].key, items[y].key) < 0; });
	for (std::size_t i = 1; i < order.size(); ++i) check_pair(items[order[i - 1]], items[order[i]], encs[order[i - 1]], encs[order[i]]);
}

// Add a value in every type that holds it exactly.
bool double_exact(std::uint64_t mag) { return mag < (std::uint64_t{1} << 53) || std::bit_width(mag) - std::countr_zero(mag) <= 53; }
bool float_exact(std::uint64_t mag) { return mag < (std::uint64_t{1} << 24) || (std::bit_width(mag) - std::countr_zero(mag) <= 24); }

void add_double(std::vector<Item>& out, double d);

void add_i64(std::vector<Item>& out, std::int64_t v) {
	const std::uint64_t mag = v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
	const Key k = exact::from_int64(v);
	out.push_back({k, static_cast<std::uint64_t>(v), I64});
	if (v >= 0) out.push_back({k, static_cast<std::uint64_t>(v), U64});
	if (double_exact(mag)) out.push_back({k, std::bit_cast<std::uint64_t>(static_cast<double>(v)), F64});
	if (float_exact(mag)) out.push_back({k, std::bit_cast<std::uint32_t>(static_cast<float>(v)), F32});
}

void add_u64(std::vector<Item>& out, std::uint64_t v) {
	if (v <= static_cast<std::uint64_t>(INT64_MAX)) {
		add_i64(out, static_cast<std::int64_t>(v));
		return;
	}
	const Key k = exact::from_uint64(v);
	out.push_back({k, v, U64});
	if (double_exact(v)) out.push_back({k, std::bit_cast<std::uint64_t>(static_cast<double>(v)), F64});
	if (float_exact(v)) out.push_back({k, std::bit_cast<std::uint32_t>(static_cast<float>(v)), F32});
}

void add_double(std::vector<Item>& out, double d) {
	const Key k = exact::from_double(d);
	out.push_back({k, std::bit_cast<std::uint64_t>(d), F64});
	if (std::isnan(d) || std::isinf(d)) {
		out.push_back({k, std::bit_cast<std::uint32_t>(static_cast<float>(d)), F32});
		return;
	}
	if (static_cast<double>(static_cast<float>(d)) == d) out.push_back({k, std::bit_cast<std::uint32_t>(static_cast<float>(d)), F32});
	if (d == std::trunc(d)) {
		if (d >= -0x1p63 && d < 0x1p63) {
			const auto v = static_cast<std::int64_t>(d);
			out.push_back({k, static_cast<std::uint64_t>(v), I64});
			if (v >= 0) out.push_back({k, static_cast<std::uint64_t>(v), U64});
		} else if (d >= 0 && d < 0x1p64) {
			out.push_back({k, static_cast<std::uint64_t>(d), U64});
		}
	}
}

void signed_both(std::vector<Item>& out, double d) {
	add_double(out, d);
	add_double(out, -d);
}

// The T1 boundary set that is not the dense integer range.
std::vector<Item> boundary_set() {
	std::vector<Item> s;
	// 2^k + d and -(2^k) + d for k 0..63, d in [-1024, 1024].
	auto add_wide = [&s](__int128 v) {
		if (v >= INT64_MIN && v <= INT64_MAX) add_i64(s, static_cast<std::int64_t>(v));
		else if (v > 0 && v <= static_cast<__int128>(UINT64_MAX)) add_u64(s, static_cast<std::uint64_t>(v));
	};
	for (int k = 0; k < 64; ++k) {
		for (int d = -1024; d <= 1024; ++d) {
			add_wide((static_cast<__int128>(1) << k) + d);
			add_wide(-(static_cast<__int128>(1) << k) + d);
		}
	}
	// Every double 2^e with both neighbours, both signs.
	for (int e = -1074; e <= 1023; ++e) {
		const double p = std::ldexp(1.0, e);
		for (double v : {p, std::nextafter(p, HUGE_VAL), std::nextafter(p, 0.0)}) signed_both(s, v);
	}
	// A fraction of every length at the window's edges, and the integers' top exponents.
	for (int e : {-64, -63, -62, -61, -60, -59, -58, -57, 60, 61, 62, 63, 64, 65, 66, 100, 1000, -1000}) {
		for (int len = 1; len <= 52; ++len) {
			const double lo = std::ldexp(1.0 + std::ldexp(1.0, -len), e);   // the lowest bit of the fraction
			const double hi = std::ldexp(2.0 - std::ldexp(1.0, -len), e);   // all ones
			const double mid = std::ldexp(1.0 + 0.5 + std::ldexp(1.0, -len), e);
			if (std::isfinite(lo)) signed_both(s, lo);
			if (std::isfinite(hi)) signed_both(s, hi);
			if (std::isfinite(mid) && len > 1) signed_both(s, mid);
		}
	}
	for (int e = 55; e <= 63; ++e) {
		for (int len = 1; len <= e; ++len) {  // integers with a fraction of up to 63 bits
			const std::uint64_t lo = (std::uint64_t{1} << e) | (std::uint64_t{1} << (e - len));
			const std::uint64_t hi = e == 63 ? ~((std::uint64_t{1} << (e - len)) - 1) : ((std::uint64_t{1} << (e + 1)) - 1) & ~((std::uint64_t{1} << (e - len)) - 1);
			for (std::uint64_t v : {lo, hi}) {
				add_u64(s, v);
				if (v <= (std::uint64_t{1} << 63)) add_i64(s, static_cast<std::int64_t>(std::uint64_t{0} - v));
			}
		}
	}
	// Runs of zeros and ones: 1 0^r 1 and 1^r, as integers and, scaled, as doubles.
	for (int r = 0; r < 63; ++r) {
		add_u64(s, (std::uint64_t{1} << (r + 1)) | 1);
		add_i64(s, -static_cast<std::int64_t>((std::uint64_t{1} << (r + 1)) | 1));
	}
	for (int r = 1; r <= 64; ++r) {
		const std::uint64_t v = r == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << r) - 1;
		add_u64(s, v);
		if (v <= static_cast<std::uint64_t>(INT64_MAX)) add_i64(s, -static_cast<std::int64_t>(v));
	}
	for (int scale : {-1074, -300, -70, -61, -30, 0, 30, 61, 70, 300, 970}) {
		for (int r = 0; r <= 51; ++r) {
			signed_both(s, std::ldexp(1.0 + std::ldexp(1.0, -r - 1), scale));
			signed_both(s, std::ldexp(2.0 - std::ldexp(1.0, -r), scale));
		}
	}
	// Specials and extremes.
	for (double d : {0.0, -0.0, HUGE_VAL, -HUGE_VAL, std::nan(""), std::bit_cast<double>(UINT64_C(0xFFF8000000000123)), DBL_MAX, -DBL_MAX, DBL_MIN, -DBL_MIN,
					 std::nextafter(DBL_MIN, 0.0), 5e-324, -5e-324, double{FLT_MAX}, double{FLT_MIN}, 0x1p63, 0x1p64, -0x1p63, 0.1, -0.1, 1e-30, 0.01, 0.0005}) {
		add_double(s, d);
	}
	add_i64(s, INT64_MIN);
	add_i64(s, INT64_MAX);
	add_u64(s, UINT64_MAX);
	add_u64(s, std::uint64_t{1} << 63);
	return s;
}

// The dense range: every int64 in [-2^20, 2^20], consecutive values strictly increasing, every type that holds
// it equal.
void dense_range() {
	std::string prev;
	for (std::int64_t v = -(1 << 20); v <= (1 << 20); ++v) {
		const std::string e = sortable::encode(v);
		auto n = sortable::decode(e);
		CHECK_MSG(n && n->to_int64() == v && exact::reencode(*n) == e, "%lld", static_cast<long long>(v));
		CHECK_MSG(e == sortable::encode(static_cast<double>(v)) && e == sortable::encode(static_cast<float>(v)), "%lld cross-type", static_cast<long long>(v));
		if (v >= 0) CHECK_MSG(e == sortable::encode(static_cast<std::uint64_t>(v)) && e == sortable::encode(static_cast<std::uint32_t>(v)), "%lld unsigned", static_cast<long long>(v));
		if (v >= INT32_MIN && v <= INT32_MAX) CHECK(e == sortable::encode(static_cast<std::int32_t>(v)));
		if (v >= -32768 && v <= 32767) CHECK(e == sortable::encode(static_cast<std::int16_t>(v)));
		if (!prev.empty()) CHECK_MSG(exact::memcmp_order(prev, e) < 0, "%lld not above its predecessor", static_cast<long long>(v));
		prev = e;
	}
	// Half-integers between them.
	for (std::int64_t v = -(1 << 16); v < (1 << 16); ++v) {
		const std::string a = sortable::encode(static_cast<double>(v)), b = sortable::encode(v + 0.5), c = sortable::encode(static_cast<double>(v + 1));
		CHECK_MSG(exact::memcmp_order(a, b) < 0 && exact::memcmp_order(b, c) < 0, "%lld.5", static_cast<long long>(v));
	}
}

struct Rng {
	std::mt19937_64 g;
	explicit Rng(std::uint64_t seed) : g(seed) {}
	std::uint64_t u64() { return g(); }
	std::int64_t i64() { return static_cast<std::int64_t>(g()); }
	// Log-uniform magnitude: every bit length is as likely as any other.
	std::uint64_t magnitude() { return g() >> (g() % 64); }
	double dbl() { return std::bit_cast<double>(g()); }
	double scaled() {  // an integer-valued or short-fraction double of any magnitude
		return std::ldexp(static_cast<double>(g() >> (g() % 64)), static_cast<int>(g() % 140) - 70) * ((g() & 1) ? -1 : 1);
	}
};

Item random_item(Rng& r) {
	std::vector<Item> one;
	switch (r.u64() % 7) {
		case 0: add_i64(one, r.i64()); break;
		case 1: add_u64(one, r.u64()); break;
		case 2: add_double(one, r.dbl()); break;
		case 3: add_i64(one, (r.u64() & 1) ? -static_cast<std::int64_t>(r.magnitude() >> 1) : static_cast<std::int64_t>(r.magnitude() >> 1)); break;
		case 4: add_u64(one, r.magnitude()); break;
		case 5: add_double(one, r.scaled()); break;
		default: add_double(one, static_cast<double>(static_cast<float>(r.dbl()))); break;
	}
	return one[r.u64() % one.size()];
}

void random_type(Rng& r, std::uint64_t count, int which) {
	std::vector<Item> chunk;
	for (std::uint64_t done = 0; done < count;) {
		chunk.clear();
		const std::uint64_t n = std::min<std::uint64_t>(count - done, 1u << 20);
		for (std::uint64_t i = 0; i < n; ++i) {
			if (which == 0) add_i64(chunk, r.i64());
			else if (which == 1) add_u64(chunk, r.u64());
			else add_double(chunk, r.dbl());
		}
		check_set(chunk);
		done += n;
	}
}

// T2's cross-type pairs: independent values and near neighbours, any mix of types.
void random_pairs(Rng& r, std::uint64_t count) {
	for (std::uint64_t i = 0; i < count; ++i) {
		Item a = random_item(r), b = random_item(r);
		if ((r.u64() & 3) == 0) {  // a near neighbour of a, in another type when it has one
			std::vector<Item> near;
			const double d = (a.kind == F64) ? std::bit_cast<double>(a.raw) : (a.kind == F32 ? static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(a.raw))) : 0.0);
			switch (r.u64() % 4) {
				case 0: if (a.kind == I64) add_i64(near, static_cast<std::int64_t>(a.raw) + static_cast<std::int64_t>(r.u64() % 5) - 2); else near.push_back(b); break;
				case 1: if (a.kind == U64 && a.raw < UINT64_MAX - 2) add_u64(near, a.raw + r.u64() % 3); else near.push_back(b); break;
				case 2: if (std::isfinite(d)) add_double(near, std::nextafter(d, (r.u64() & 1) ? HUGE_VAL : -HUGE_VAL)); else near.push_back(b); break;
				default: near.push_back(b); break;
			}
			b = near[r.u64() % near.size()];
		}
		check_pair(a, b, bytes_of(a), bytes_of(b));
		++g_checked;
	}
}

}  // namespace

int main(int argc, char** argv) {
	std::uint64_t seed = 20261027, random = 3'000'000;
	if (const char* s = std::getenv("SORTABLE_SEED")) seed = std::strtoull(s, nullptr, 10);
	if (const char* s = std::getenv("SORTABLE_RANDOM")) random = std::strtoull(s, nullptr, 10);
	if (argc > 1) random = std::strtoull(argv[1], nullptr, 10);
	std::printf("seed %llu, %llu random values per type\n", static_cast<unsigned long long>(seed), static_cast<unsigned long long>(random));

	dense_range();
	std::printf("dense range checked\n");
	{
		auto s = boundary_set();
		std::printf("boundary set: %zu values\n", s.size());
		check_set(s);
	}
	Rng r(seed);
	for (int which = 0; which < 3; ++which) random_type(r, random, which);
	random_pairs(r, random);
	std::printf("checked %zu values, %zu ordered pairs\n", g_checked, g_pairs);
	if (check::failures) {
		std::printf("%d failures (seed %llu)\n", check::failures, static_cast<unsigned long long>(seed));
		return 1;
	}
	std::printf("all sortable property tests passed\n");
	return 0;
}
