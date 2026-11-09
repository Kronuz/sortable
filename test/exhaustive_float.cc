// T2: every float (all 2^32 bit patterns): each non-NaN in value order round trips bit for bit (-0.0 to
// 0) with strictly increasing bytes, and every NaN encodes as 0xff.
// Not part of CI (minutes): build with -DSORTABLE_EXHAUSTIVE=ON and run sortable_exhaustive_float.
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "check.hh"
#include "exact.hh"
#include "sortable.hh"

namespace {

std::string prev;
std::uint64_t checked = 0;
bool have_prev = false;

void visit(float f) {
	char buf[sortable::max_size];
	const std::size_t n = sortable::encode(f, buf);
	const std::string_view e(buf, n);
	auto d = sortable::decode(e);
	CHECK_MSG(d.has_value(), "%08x", std::bit_cast<std::uint32_t>(f));
	if (d) {
		const double back = d->to_double();
		if (std::isnan(f)) CHECK(std::isnan(back));
		else CHECK_MSG(back == static_cast<double>(f) && (f == 0 || std::bit_cast<std::uint32_t>(static_cast<float>(back)) == std::bit_cast<std::uint32_t>(f)), "%08x", std::bit_cast<std::uint32_t>(f));
	}
	if (have_prev) CHECK_MSG(exact::memcmp_order(prev, e) < 0, "%08x not above its predecessor", std::bit_cast<std::uint32_t>(f));
	prev.assign(e);
	have_prev = true;
	++checked;
}

}  // namespace

int main() {
	visit(-HUGE_VALF);
	for (std::uint32_t b = 0xFF7FFFFF; b >= 0x80000001u; --b) visit(std::bit_cast<float>(b));  // -FLT_MAX .. -least subnormal
	visit(0.0f);
	// -0.0 encodes as 0 and decodes to 0.
	CHECK(sortable::encode(-0.0f) == sortable::encode(0.0f));
	++checked;
	for (std::uint32_t b = 1; b <= 0x7F7FFFFFu; ++b) visit(std::bit_cast<float>(b));
	visit(HUGE_VALF);
	// Every NaN, either sign, is the single byte 0xff, above +inf, and decodes to NaN.
	const auto nan = sortable::decode("\xff");
	CHECK(nan && nan->is_nan() && std::isnan(nan->to_double()));
	for (std::uint32_t b = 0x7F800001u; b <= 0x7FFFFFFFu; ++b) {
		for (const std::uint32_t s : {b, b | 0x80000000u}) {
			char buf[sortable::max_size];
			const std::size_t n = sortable::encode(std::bit_cast<float>(s), buf);
			CHECK_MSG(n == 1 && buf[0] == '\xff', "%08x", s);
			++checked;
		}
	}
	CHECK(exact::memcmp_order(prev, "\xff") < 0);
	std::printf("checked %llu floats\n", static_cast<unsigned long long>(checked));
	CHECK(checked == std::uint64_t{1} << 32);
	if (check::failures) return 1;
	std::printf("all floats passed\n");
	return 0;
}
