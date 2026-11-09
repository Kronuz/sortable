// The codec over stdin lines, for cross-language tests: sortable_cli encode | decode.
//   encode: "i DECIMAL" (an integer in [-2^63, 2^64)) or "f HEX16" (binary64 bits) -> the encoding's hex
//   decode: an input's hex, an empty line for the empty input -> "reject", or the value as encode reads it
// decode writes an integer in [-2^63, 2^64) as "i", anything else as "f" (every NaN as 7ff8000000000000).
// A malformed line names itself on stderr and exits 2; a write error exits 1.
#include <bit>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#include "sortable.hh"

namespace {

constexpr char digits[] = "0123456789abcdef";

[[noreturn]] void malformed(std::uint64_t line, std::string_view text, const char* why) {
	std::cout.flush();
	std::fprintf(stderr, "sortable_cli: line %llu: %s: \"%.*s\"\n", static_cast<unsigned long long>(line), why,
		static_cast<int>(text.size()), text.data());
	std::exit(2);
}

int nibble(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// The whole of `s` as an unsigned number in `base`, or false.
bool parse(std::string_view s, int base, std::uint64_t& v) {
	if (s.empty()) return false;
	const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v, base);
	return ec == std::errc{} && end == s.data() + s.size();
}

void put_hex(std::string& out, std::string_view bytes) {
	for (const unsigned char c : bytes) {
		out += digits[c >> 4];
		out += digits[c & 15];
	}
}

void put_bits(std::string& out, double d) {
	const auto bits = std::bit_cast<std::uint64_t>(d);
	for (int shift = 60; shift >= 0; shift -= 4) out += digits[(bits >> shift) & 15];
}

void encode_line(std::uint64_t line, std::string_view text, std::string& out) {
	char buf[sortable::max_size];
	std::size_t n = 0;
	std::uint64_t v = 0;
	if (text.starts_with("i ")) {
		std::string_view num = text.substr(2);
		const bool negative = num.starts_with('-');
		if (negative) num.remove_prefix(1);
		if (!parse(num, 10, v) || (negative && v > std::uint64_t{1} << 63)) malformed(line, text, "not an integer in [-2^63, 2^64)");
		n = negative ? sortable::encode(static_cast<std::int64_t>(std::uint64_t{0} - v), buf) : sortable::encode(v, buf);
	} else if (text.starts_with("f ")) {
		const std::string_view hex = text.substr(2);
		if (hex.size() != 16 || !parse(hex, 16, v)) malformed(line, text, "not 16 hex digits of binary64 bits");
		n = sortable::encode(std::bit_cast<double>(v), buf);
	} else {
		malformed(line, text, "expected \"i DECIMAL\" or \"f HEX16\"");
	}
	put_hex(out, {buf, n});
}

void decode_line(std::uint64_t line, std::string_view text, std::string& bytes, std::string& out) {
	if (text.size() % 2 != 0) malformed(line, text, "odd number of hex digits");
	bytes.clear();
	for (std::size_t i = 0; i < text.size(); i += 2) {
		const int hi = nibble(text[i]), lo = nibble(text[i + 1]);
		if (hi < 0 || lo < 0) malformed(line, text, "not hex");
		bytes += static_cast<char>((hi << 4) | lo);
	}
	const auto n = sortable::decode(bytes);
	if (!n) {
		out += "reject";
	} else if (const auto i = n->to_int64()) {
		out += "i ";
		out += std::to_string(*i);
	} else if (const auto u = n->to_uint64()) {
		out += "i ";
		out += std::to_string(*u);
	} else {
		out += "f ";
		put_bits(out, n->to_double());
	}
}

}  // namespace

int main(int argc, char** argv) {
	const std::string_view mode = argc == 2 ? argv[1] : "";
	if (mode != "encode" && mode != "decode") {
		std::fprintf(stderr, "usage: sortable_cli encode|decode < lines\n");
		return 2;
	}
	std::ios::sync_with_stdio(false);
	std::string text, bytes, out;
	std::uint64_t line = 0;
	while (std::getline(std::cin, text)) {
		++line;
		out.clear();
		if (mode == "encode") encode_line(line, text, out);
		else decode_line(line, text, bytes, out);
		out += '\n';
		std::cout << out;
	}
	if (std::cin.bad()) {
		std::fprintf(stderr, "sortable_cli: read error\n");
		return 1;
	}
	std::cout.flush();
	if (!std::cout) {
		std::fprintf(stderr, "sortable_cli: write error\n");
		return 1;
	}
	return 0;
}
