// Unit tests: the format's examples, the API's call forms, the typed conversions, the model's vectors
// (T6), the reject fixture and the exhaustive short inputs (T4), concurrent use.
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "check.hh"
#include "exact.hh"
#include "sortable.hh"

#ifndef SORTABLE_TEST_DIR
#define SORTABLE_TEST_DIR "."
#endif

// The call forms of the API (T11): these compile on every platform, or the build fails.
template <class T>
inline constexpr bool can_encode = requires(T v, char* p) {
	sortable::encode(v, p);
	sortable::encode(v);
};
#define OK(expr) static_assert(can_encode<decltype(expr)>)
#define NO(expr) static_assert(!can_encode<decltype(expr)>)
OK(5);
OK(5L);
OK(5LL);
OK(5u);
OK(5UL);
OK(5ULL);
OK(std::size_t{5});
OK(std::ptrdiff_t{5});
OK(std::int8_t{5});
OK(std::uint8_t{5});
OK(std::int16_t{5});
OK(std::uint16_t{5});
OK(std::int32_t{5});
OK(std::uint32_t{5});
OK(std::int64_t{5});
OK(std::uint64_t{5});
OK(static_cast<signed char>(5));
OK(static_cast<unsigned char>(5));
OK(5.0f);
OK(5.0);
NO(true);
NO('a');
NO(L'a');
NO(u8'a');
NO(u'a');
NO(U'a');
NO(5.0L);

// Fixed encodings, evaluated at compile time (which also rules out undefined behavior in them).
template <class T>
constexpr bool is(T v, std::string_view hex) {
	char buf[sortable::max_size];
	const std::size_t n = sortable::encode(v, buf);
	if (n * 2 != hex.size()) return false;
	auto nib = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
	for (std::size_t i = 0; i < n; ++i) {
		if (static_cast<unsigned char>(buf[i]) != nib(hex[2 * i]) * 16 + nib(hex[2 * i + 1])) return false;
	}
	return true;
}

#ifndef SORTABLE_MUTANT_RUN  // the mutant runs leave the checking to the run-time tests
static_assert(is(0, "80") && is(1, "be") && is(-1, "41") && is(3, "bf80") && is(1.5, "be80"));
static_assert(is(5, "c040") && is(5u, "c040") && is(5.0, "c040") && is(5.0f, "c040"));
static_assert(is(std::int64_t{2147483649}, "dd00000002") && is(std::int64_t{-1073741824}, "23"));
static_assert(is(std::int64_t{-1073741825}, "22fffffffc") && is(std::int64_t{1761523200}, "dca3fad8"));
static_assert(is(0.01, "b747ae147ae147b0") && is(0.0005, "b30624dd2f1a9fc0"));
static_assert(is(std::uint64_t{1} << 60, "fa") && is(~std::uint64_t{0}, "fdfffffffffffffffe"));
static_assert(is(std::numeric_limits<std::int64_t>::min(), "02") && is(1e-30, "8103ce4484bfeebc2a"));
static_assert(is(DBL_MAX, "fe03bffffffffffffff0") && is(-DBL_MAX, "01fc3f00000000000010"));
static_assert(is(HUGE_VAL, "feffff") && is(-HUGE_VAL, "010000") && is(-0.0, "80"));
static_assert(is(std::numeric_limits<double>::quiet_NaN(), "ff") && is(-std::numeric_limits<double>::quiet_NaN(), "ff"));
static_assert(is(5e-324, "810000") && is(-5e-324, "7effff"));
static_assert(sortable::decode(std::string_view("\xbe", 1)).has_value());
static_assert(!sortable::decode(std::string_view()).has_value());
#endif

namespace {

using sortable::number;

std::string unhex(const std::string& h) {
	std::string r;
	if (h == "-") return r;
	for (std::size_t i = 0; i + 1 < h.size(); i += 2) r += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
	return r;
}

// A copy sized exactly to the input, so a read past the end is an ASan finding.
std::optional<number> decode_exact(const std::string& s) {
	auto p = std::make_unique<char[]>(s.size());
	if (!s.empty()) std::memcpy(p.get(), s.data(), s.size());
	return sortable::decode(std::string_view(p.get(), s.size()));
}

void examples() {
	struct Ex {
		std::string bytes;
		std::string hex;
	};
	const Ex table[] = {
		{sortable::encode(0), "80"},
		{sortable::encode(1), "be"},
		{sortable::encode(-1), "41"},
		{sortable::encode(3), "bf80"},
		{sortable::encode(1.5), "be80"},
		{sortable::encode(std::int64_t{2147483649}), "dd00000002"},
		{sortable::encode(std::int64_t{-1073741824}), "23"},
		{sortable::encode(std::int64_t{-1073741825}), "22fffffffc"},
		{sortable::encode(std::int64_t{1761523200}), "dca3fad8"},
		{sortable::encode(0.01), "b747ae147ae147b0"},
		{sortable::encode(0.0005), "b30624dd2f1a9fc0"},
		{sortable::encode(std::uint64_t{1} << 60), "fa"},
		{sortable::encode(UINT64_MAX), "fdfffffffffffffffe"},
		{sortable::encode(INT64_MIN), "02"},
		{sortable::encode(1e-30), "8103ce4484bfeebc2a"},
		{sortable::encode(DBL_MAX), "fe03bffffffffffffff0"},
		{sortable::encode(-DBL_MAX), "01fc3f00000000000010"},
		{sortable::encode(HUGE_VAL), "feffff"},
		{sortable::encode(-HUGE_VAL), "010000"},
		{sortable::encode(std::nan("")), "ff"},
	};
	for (const auto& e : table) CHECK_MSG(exact::hex(e.bytes) == e.hex, "%s != %s", exact::hex(e.bytes).c_str(), e.hex.c_str());

	// The old codec's failures, now exact.
	CHECK(sortable::encode(std::int64_t{2147483649}) < sortable::encode(std::int64_t{2147483650}));
	CHECK(sortable::encode(std::int64_t{-1073741825}) < sortable::encode(std::int64_t{-1073741824}));
	CHECK(sortable::encode(1.0) < sortable::encode(1.0 + 0x1p-40));
	// Cross-type equality.
	CHECK(sortable::encode(5) == sortable::encode(5u) && sortable::encode(5u) == sortable::encode(5.0));
	CHECK(sortable::encode(std::int8_t{-7}) == sortable::encode(-7.0f));
	CHECK(sortable::encode(UINT64_C(9007199254740993)) == sortable::encode(INT64_C(9007199254740993)));
	// 2^53 + 1 is not a double: it sits between 2^53 and 2^53 + 2.
	CHECK(sortable::encode(UINT64_C(9007199254740992)) < sortable::encode(UINT64_C(9007199254740993)));
	CHECK(sortable::encode(UINT64_C(9007199254740993)) < sortable::encode(9007199254740994.0));
	// -0.0 is 0; every NaN is 0xff and sorts last.
	CHECK(sortable::encode(-0.0) == sortable::encode(0));
	CHECK(sortable::encode(std::bit_cast<double>(UINT64_C(0xFFF8000000000123))) == "\xff");
	CHECK(sortable::encode(HUGE_VAL) < sortable::encode(std::nan("")));
	CHECK(sortable::encode(-HUGE_VAL) < sortable::encode(-DBL_MAX));
	// The sentinels.
	CHECK(sortable::max_finite == sortable::encode(DBL_MAX) && sortable::min_finite == sortable::encode(-DBL_MAX));
	CHECK(sortable::encode(UINT64_MAX) < sortable::max_finite && sortable::encode(DBL_MAX) == sortable::max_finite);
	CHECK(sortable::max_finite < sortable::encode(HUGE_VAL) && sortable::encode(-HUGE_VAL) < sortable::min_finite);
	CHECK(sortable::min_finite < sortable::encode(INT64_MIN));
	// A shorter buffer variant writes the same bytes.
	char buf[sortable::max_size];
	CHECK(std::string(buf, sortable::encode(1e-30, buf)) == sortable::encode(1e-30));
	CHECK(sortable::encode(5e-324).size() == 3 && sortable::encode(1e-30).size() == 9);
	CHECK(sortable::encode(-1e-30).size() == 9 && sortable::max_size == 10);
}

void conversions() {
	auto dec = [](const std::string& s) { return decode_exact(s).value(); };
	CHECK(dec(sortable::encode(5)).to_int64() == 5);
	CHECK(dec(sortable::encode(5)).to_uint64() == 5);
	CHECK(dec(sortable::encode(-5)).to_int64() == -5);
	CHECK(!dec(sortable::encode(-5)).to_uint64());
	CHECK(dec(sortable::encode(INT64_MIN)).to_int64() == INT64_MIN);
	CHECK(!dec(sortable::encode(INT64_MIN)).to_uint64());
	CHECK(dec(sortable::encode(INT64_MAX)).to_int64() == INT64_MAX);
	CHECK(!dec(sortable::encode(UINT64_C(1) << 63)).to_int64());
	CHECK(dec(sortable::encode(UINT64_C(1) << 63)).to_uint64() == UINT64_C(1) << 63);
	CHECK(dec(sortable::encode(UINT64_MAX)).to_uint64() == UINT64_MAX);
	CHECK(!dec(sortable::encode(1.5)).to_int64() && !dec(sortable::encode(0.5)).to_uint64());
	CHECK(dec(sortable::encode(2.0)).to_int64() == 2 && dec(sortable::encode(0)).to_int64() == 0);
	CHECK(!dec(sortable::encode(1e30)).to_int64() && !dec(sortable::encode(HUGE_VAL)).to_int64());
	CHECK(!dec(sortable::encode(std::nan(""))).to_int64());
	// Saturation: truncate toward zero, clamp, NaN is 0.
	CHECK(dec(sortable::encode(1.9)).saturate_int64() == 1 && dec(sortable::encode(-1.9)).saturate_int64() == -1);
	CHECK(dec(sortable::encode(0.9)).saturate_int64() == 0 && dec(sortable::encode(-0.9)).saturate_int64() == 0);
	CHECK(dec(sortable::encode(1e30)).saturate_int64() == INT64_MAX && dec(sortable::encode(-1e30)).saturate_int64() == INT64_MIN);
	CHECK(dec(sortable::encode(HUGE_VAL)).saturate_int64() == INT64_MAX && dec(sortable::encode(-HUGE_VAL)).saturate_int64() == INT64_MIN);
	CHECK(dec(sortable::encode(std::nan(""))).saturate_int64() == 0 && dec(sortable::encode(std::nan(""))).saturate_uint64() == 0);
	CHECK(dec(sortable::encode(UINT64_MAX)).saturate_int64() == INT64_MAX);
	CHECK(dec(sortable::encode(INT64_MIN)).saturate_int64() == INT64_MIN);
	CHECK(dec(sortable::encode(-4.5)).saturate_int64() == -4);
	CHECK(dec(sortable::encode(-1.0)).saturate_uint64() == 0 && dec(sortable::encode(-HUGE_VAL)).saturate_uint64() == 0);
	CHECK(dec(sortable::encode(4.5)).saturate_uint64() == 4 && dec(sortable::encode(1e30)).saturate_uint64() == UINT64_MAX);
	CHECK(dec(sortable::encode(HUGE_VAL)).saturate_uint64() == UINT64_MAX && dec(sortable::encode(0.5)).saturate_uint64() == 0);
	CHECK(dec(sortable::encode(1.8446744073709552e19)).saturate_uint64() == UINT64_MAX);  // 2^64
	// to_double: exact for doubles, nearest-even for integers above 2^53.
	CHECK(dec(sortable::encode(0.1)).to_double() == 0.1 && dec(sortable::encode(-DBL_MAX)).to_double() == -DBL_MAX);
	CHECK(dec(sortable::encode(5e-324)).to_double() == 5e-324 && dec(sortable::encode(-5e-324)).to_double() == -5e-324);
	CHECK(dec(sortable::encode(DBL_MIN)).to_double() == DBL_MIN && dec(sortable::encode(2.5e-310)).to_double() == 2.5e-310);
	CHECK(dec(sortable::encode(0)).to_double() == 0 && !std::signbit(dec(sortable::encode(0)).to_double()));
	CHECK(dec(sortable::encode(HUGE_VAL)).to_double() == HUGE_VAL && dec(sortable::encode(-HUGE_VAL)).to_double() == -HUGE_VAL);
	CHECK(std::isnan(dec(sortable::encode(std::nan(""))).to_double()));
	CHECK(dec(sortable::encode(UINT64_C(9007199254740993))).to_double() == 9007199254740992.0);   // tie to even
	CHECK(dec(sortable::encode(UINT64_C(9007199254740995))).to_double() == 9007199254740996.0);   // tie to even
	CHECK(dec(sortable::encode(UINT64_C(9007199254740994))).to_double() == 9007199254740994.0);
	CHECK(dec(sortable::encode(UINT64_C(9007199254740997))).to_double() == 9007199254740996.0);   // not a tie
	CHECK(dec(sortable::encode(UINT64_MAX)).to_double() == 18446744073709551616.0);                // carry into the exponent
	CHECK(dec(sortable::encode(INT64_MIN + 1)).to_double() == -9223372036854775808.0);
	CHECK(dec(sortable::encode(-INT64_C(9007199254740993))).to_double() == -9007199254740992.0);
	// Predicates.
	CHECK(dec(sortable::encode(0)).is_zero() && dec(sortable::encode(HUGE_VAL)).is_inf() && dec(sortable::encode(std::nan(""))).is_nan());
}

void vectors() {
	std::ifstream in(std::string(SORTABLE_TEST_DIR) + "/vectors.txt");
	CHECK(in.good());
	std::string line;
	std::size_t lines = 0;
	while (std::getline(in, line)) {
		std::istringstream ss(line);
		std::string kind, text, hex;
		ss >> kind >> text >> hex;
		++lines;
		std::string got;
		exact::Key key;
		if (kind == "i") {
			const auto v = static_cast<std::int64_t>(std::strtoll(text.c_str(), nullptr, 10));
			got = sortable::encode(v);
			key = exact::from_int64(v);
		} else if (kind == "u") {
			const auto v = static_cast<std::uint64_t>(std::strtoull(text.c_str(), nullptr, 10));
			got = sortable::encode(v);
			key = exact::from_uint64(v);
		} else {
			const double v = std::strtod(text.c_str(), nullptr);
			got = sortable::encode(v);
			key = exact::from_double(v);
			if (v == std::floor(v) && std::fabs(v) < 9.2e18) CHECK_MSG(sortable::encode(static_cast<std::int64_t>(v)) == got, "%s", line.c_str());
		}
		CHECK_MSG(exact::hex(got) == hex, "%s: got %s", line.c_str(), exact::hex(got).c_str());
		auto n = decode_exact(unhex(hex));
		CHECK_MSG(n.has_value(), "%s", line.c_str());
		if (n) {
			CHECK_MSG(exact::compare(exact::from_number(*n), key) == 0, "%s", line.c_str());
			CHECK_MSG(exact::reencode(*n) == unhex(hex), "%s", line.c_str());
		}
	}
	CHECK_MSG(lines >= 113000, "%zu vector lines", lines);
	std::printf("vectors: %zu lines\n", lines);
}

void reject_fixture() {
	std::ifstream in(std::string(SORTABLE_TEST_DIR) + "/reject.txt");
	CHECK(in.good());
	std::string line;
	std::size_t lines = 0;
	int per_rule[10] = {};
	while (std::getline(in, line)) {
		std::istringstream ss(line);
		std::string rule, name, hex;
		ss >> rule >> name >> hex;
		++lines;
		CHECK_MSG(!decode_exact(unhex(hex)).has_value(), "%s %s %s is accepted", rule.c_str(), name.c_str(), hex.c_str());
		++per_rule[rule.back() - '0'];
	}
	for (int r = 1; r <= 9; ++r) CHECK_MSG(per_rule[r] > 0, "no witness for D5.%d", r);
	std::printf("reject fixture: %zu witnesses\n", lines);
}

// Every input is either rejected or re-encodes to itself (T4); every accepted input is counted so a
// decoder that rejects too much shows too.
void exhaustive_short() {
	std::size_t accepted = 0, total = 0;
	auto run = [&](char* buf, std::size_t len) {
		++total;
		auto n = sortable::decode(std::string_view(buf, len));
		if (!n) return;
		++accepted;
		const std::string again = exact::reencode(*n);
		CHECK_MSG(again == std::string(buf, len), "accepted %s re-encodes to %s", exact::hex(std::string(buf, len)).c_str(), exact::hex(again).c_str());
	};
	auto b0 = std::make_unique<char[]>(1);
	auto b1 = std::make_unique<char[]>(1);
	auto b2 = std::make_unique<char[]>(2);
	auto b3 = std::make_unique<char[]>(3);
	run(b0.get(), 0);
	for (int a = 0; a < 256; ++a) {
		b1[0] = static_cast<char>(a);
		run(b1.get(), 1);
		for (int b = 0; b < 256; ++b) {
			b2[0] = static_cast<char>(a);
			b2[1] = static_cast<char>(b);
			run(b2.get(), 2);
			for (int c = 0; c < 256; ++c) {
				b3[0] = static_cast<char>(a);
				b3[1] = static_cast<char>(b);
				b3[2] = static_cast<char>(c);
				run(b3.get(), 3);
			}
		}
	}
	// Four bytes behind each escape tag, and the largest direct and escape tails.
	auto b4 = std::make_unique<char[]>(4);
	for (int a : {0x01, 0x7E, 0x81, 0xFE}) {
		for (std::uint32_t v = 0; v < (1u << 24); ++v) {
			b4[0] = static_cast<char>(a);
			b4[1] = static_cast<char>(v >> 16);
			b4[2] = static_cast<char>(v >> 8);
			b4[3] = static_cast<char>(v);
			run(b4.get(), 4);
		}
	}
	std::printf("exhaustive short: %zu inputs, %zu accepted\n", total, accepted);
	// A 1-byte input is accepted for the zero, NaN and every direct-tag power of two of either sign: 2 + 124 + 124.
	std::size_t one_byte = 0;
	for (int a = 0; a < 256; ++a) {
		b1[0] = static_cast<char>(a);
		one_byte += sortable::decode(std::string_view(b1.get(), 1)).has_value();
	}
	CHECK_MSG(one_byte == 2 + 124 + 124, "%zu accepted 1-byte inputs", one_byte);
}

void threads() {
	// Stateless: concurrent use of encode and decode agrees with the single-threaded answer.
	std::vector<std::thread> ts;
	std::vector<int> bad(4, 0);
	for (int t = 0; t < 4; ++t) {
		ts.emplace_back([t, &bad] {
			for (std::int64_t v = -20000 + t; v < 20000; v += 4) {
				const std::string s = sortable::encode(v);
				auto n = sortable::decode(s);
				if (!n || n->to_int64() != v || s != sortable::encode(static_cast<double>(v))) ++bad[t];
			}
		});
	}
	for (auto& t : ts) t.join();
	for (int b : bad) CHECK(b == 0);
}

}  // namespace

int main() {
	examples();
	conversions();
	vectors();
	reject_fixture();
	exhaustive_short();
	threads();
	if (check::failures) {
		std::printf("%d failures\n", check::failures);
		return 1;
	}
	std::printf("all sortable tests passed\n");
	return 0;
}
