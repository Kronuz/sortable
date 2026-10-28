// Encoded sizes on the design's distributions (D6, T5): mean bytes per value against the old codec's
// means recorded in docs/design/sortable-codec.md, including sets below 1/16 second (R1) and the
// deliberate cost below 2^-60 (N1). The distributions are this repository's own, seeded: they match the
// design's in shape, not value for value, so the bound allows 0.02 B.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "check.hh"
#include "sortable.hh"

namespace {

std::mt19937_64 rng(27);

double u01() { return static_cast<double>(rng() >> 11) * 0x1p-53; }

struct Set {
	const char* name;
	double old_mean;                 // the old codec's mean bytes per value (design table)
	std::function<std::string()> make;
	double slack = 0.02;
};

}  // namespace

int main() {
	using sortable::encode;
	const std::vector<Set> sets = {
		{"small ints 0..100", 1.96, [] { return encode(static_cast<int>(rng() % 101)); }},
		{"signed ints -1000..1000", 2.73, [] { return encode(static_cast<int>(rng() % 2001) - 1000); }},
		{"negative ints -1e6..0", 3.94, [] { return encode(-static_cast<int>(rng() % 1000001)); }},
		{"prices 2 decimals 0.01..1000", 7.79, [] { return encode(static_cast<double>(1 + rng() % 100000) / 100.0); }},
		{"negative prices", 7.82, [] { return encode(-static_cast<double>(1 + rng() % 100000) / 100.0); }},
		{"scores uniform [0,1)", 7.93, [] { return encode(u01()); }},
		{"epoch ms as integer", 6.75, [] { return encode(static_cast<std::int64_t>(1'500'000'000'000 + rng() % 200'000'000'000)); }},
		{"snowflake ids 2^60..2^62", 8.99, [] { return encode((std::uint64_t{1} << 60) + rng() % (std::uint64_t{3} << 60)); }},
		{"uint64 hashes", 9.24, [] { return encode(static_cast<std::uint64_t>(rng())); }},
		{"datetime ms precision", 7.99, [] { return encode(std::floor((1.5e9 + u01() * 2e8) * 1000.0) / 1000.0); }},
		{"datetime us precision", 7.98, [] { return encode(std::floor((1.5e9 + u01() * 2e8) * 1e6) / 1e6); }},
		{"datetime whole seconds", 5.00, [] { return encode(std::floor(1.5e9 + u01() * 2e8)); }},
		{"timedelta 0..3600 s, ms", 7.96, [] { return encode(std::floor(u01() * 3600.0 * 1000.0) / 1000.0); }},
		// R1: below 1/16 second, where a window that is too narrow loses (revision 0 was 24% larger).
		{"latencies 0.0005..0.06 s", 7.99, [] { return encode(0.0005 + u01() * 0.0595); }},
		{"latencies 1e-9..1e-3 s", 7.99, [] { return encode(std::exp(std::log(1e-9) + u01() * (std::log(1e-3) - std::log(1e-9)))); }},
		// N1: below 2^-60 a double takes the 3-byte escape: +24% over the old codec, accepted.
		{"deliberate: doubles 1e-30..1e-20", 7.98, [] { return encode(1e-30 + u01() * 1e-20); }, 2.5},
	};
	for (const auto& s : sets) {
		constexpr int n = 2000;
		std::size_t total = 0, longest = 0;
		for (int i = 0; i < n; ++i) {
			const std::string e = s.make();
			total += e.size();
			longest = std::max(longest, e.size());
		}
		const double mean = static_cast<double>(total) / n;
		std::printf("%-34s old %5.2f B  new %5.2f B  max %zu B\n", s.name, s.old_mean, mean, longest);
		CHECK_MSG(mean <= s.old_mean + s.slack, "%s: %.2f B against %.2f B", s.name, mean, s.old_mean);
		CHECK(longest <= sortable::max_size);
	}
	if (check::failures) return 1;
	std::printf("T5: PASS\n");
	return 0;
}
