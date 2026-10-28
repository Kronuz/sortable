// libFuzzer target for the decoder (T4): arbitrary bytes are rejected or re-encode to themselves; the
// input split in two keeps memcmp order equal to numeric order; its first eight bytes, taken as each
// type, round trip. Build: cmake -B build-fuzz -DSORTABLE_FUZZ=ON (clang), run build-fuzz/sortable_fuzz.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "../test/exact.hh"
#include "sortable.hh"

namespace {

[[noreturn]] void fail(const char* what, std::string_view a, std::string_view b = {}) {
	std::fprintf(stderr, "sortable fuzz: %s: %s %s\n", what, exact::hex(a).c_str(), exact::hex(b).c_str());
	std::abort();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view in(reinterpret_cast<const char*>(data), size);
	const auto n = sortable::decode(in);
	if (n && exact::reencode(*n) != in) fail("accepted, not canonical", in);

	if (size >= 1) {
		const std::size_t cut = data[0] % size;
		const std::string_view a = in.substr(1, cut), b = in.substr(1 + cut);
		const auto x = sortable::decode(a), y = sortable::decode(b);
		if (x && y && exact::compare(exact::from_number(*x), exact::from_number(*y)) != exact::memcmp_order(a, b)) fail("order", a, b);
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
