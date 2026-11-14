// Times encode and typed decode separately; input generation and allocation stay outside timings.
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "sortable.hh"

namespace {
volatile std::uint64_t sink = 0;
struct encoded { std::array<char, sortable::max_size> bytes; std::size_t size; };
using clock_type = std::chrono::steady_clock;
#if defined(__GNUC__) || defined(__clang__)
template<class T> inline void escape(const T& value) { asm volatile("" : : "g"(&value) : "memory"); }
#else
template<class T> inline void escape(const T& value) { sink = sink ^ sizeof(value); }
#endif

template<class T> void measure(const std::string& name, const std::vector<T>& values, int passes) {
  if (values.empty()) return;
  std::vector<encoded> bytes(values.size());
  std::size_t total = 0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    bytes[i].size = sortable::encode(values[i], bytes[i].bytes.data());
    total += bytes[i].size;
    const auto decoded = sortable::decode({bytes[i].bytes.data(), bytes[i].size});
    if (!decoded) throw std::runtime_error("benchmark input failed decoding");
  }
  const char* type = std::is_same_v<T, double> ? "double" : std::is_signed_v<T> ? "int64" : "uint64";
  if (passes == 0) { std::printf("%s\t%s\t%zu\tcheck\t-\t-\t%.4f\n", name.c_str(), type, values.size(), double(total)/values.size()); return; }
  for (int sample = -1; sample < 7; ++sample) {
    const auto start = clock_type::now();
    for (int repeat = 0; repeat < passes; ++repeat) {
      for (const T value : values) { char out[sortable::max_size]; const auto n = sortable::encode(value, out); escape(out); escape(n); }
    }
    const auto middle = clock_type::now();
    for (int repeat = 0; repeat < passes; ++repeat) {
      for (const auto& b : bytes) {
        const auto n = sortable::decode({b.bytes.data(), b.size});
        if (!n) std::abort();
        if constexpr (std::is_same_v<T, double>) { const auto value = n->to_double(); escape(value); }
        else if constexpr (std::is_signed_v<T>) { const auto value = n->to_int64(); escape(value); }
        else { const auto value = n->to_uint64(); escape(value); }
      }
    }
    const auto end = clock_type::now();
    if (sample >= 0) std::printf("%s\t%s\t%zu\t%d\t%.4f\t%.4f\t%.4f\n", name.c_str(), type, values.size(), sample,
      std::chrono::duration<double, std::nano>(middle-start).count() / (passes * values.size()),
      std::chrono::duration<double, std::nano>(end-middle).count() / (passes * values.size()), double(total)/values.size());
  }
}
}
int main(int argc, char** argv) {
  try {
    const int passes = argc > 2 ? std::stoi(argv[2]) : 20;
    if (passes < 0) throw std::runtime_error("passes must be nonnegative (0 verifies without timing)");
    std::puts("set\ttype\tvalues\tsample\tencode_ns\tdecode_ns\tbytes");
    std::mt19937_64 rng(611);
    for (int width : {8, 16, 32, 64}) {
      std::vector<std::int64_t> si; std::vector<std::uint64_t> ui;
      const auto mask = width == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << width)-1;
      for (int i=0; i<10000; ++i) {
        const auto v = rng() & mask;
        ui.push_back(v);
        const auto signed_bits = width == 64 ? v : ((v ^ (std::uint64_t{1} << (width-1))) - (std::uint64_t{1} << (width-1)));
        si.push_back(std::bit_cast<std::int64_t>(signed_bits));
      }
      measure("uniform-"+std::to_string(width), si, passes); measure("uniform-"+std::to_string(width), ui, passes);
    }
    std::vector<double> doubles;
    for (int i=0; i<10000; ++i) doubles.push_back(std::bit_cast<double>(rng()));
    measure("uniform-binary64-bits", doubles, passes);
    if (argc > 1 && std::string(argv[1]) != "-") {
      std::ifstream input(argv[1]);
      if (!input) throw std::runtime_error("cannot open corpus");
      std::string line, name;
      std::vector<std::int64_t> si; std::vector<std::uint64_t> ui; std::vector<double> ds;
      auto flush = [&] { measure(name,si,passes); measure(name,ui,passes); measure(name,ds,passes); si.clear(); ui.clear(); ds.clear(); };
      while (std::getline(input,line)) {
        const auto a=line.find('\t'), b=line.find('\t',a+1);
        if(a==std::string::npos || b==std::string::npos) throw std::runtime_error("expected set TAB kind TAB value");
        if (name != line.substr(0,a)) { flush(); name=line.substr(0,a); }
        const auto kind=line.substr(a+1,b-a-1), text=line.substr(b+1);
        if(kind=="i") { const auto v=std::stoll(text); si.push_back(v); if(v>=0) ui.push_back(v); ds.push_back(static_cast<double>(v)); }
        else if(kind=="u") { const auto v=std::stoull(text); ui.push_back(v); ds.push_back(static_cast<double>(v)); }
        else if(kind=="d") ds.push_back(std::stod(text));
        else throw std::runtime_error("unknown corpus kind");
      }
      flush();
    }
  } catch(const std::exception& e) { std::fprintf(stderr,"sortable_speed: %s\n",e.what()); return 1; }
}
