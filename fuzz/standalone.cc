// Replays files through the fuzz target where libFuzzer is not available: standalone_fuzz FILE...
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		std::ifstream in(argv[i], std::ios::binary);
		std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		auto copy = std::vector<std::uint8_t>(bytes.begin(), bytes.end());
		LLVMFuzzerTestOneInput(copy.data(), copy.size());
	}
	std::printf("replayed %d inputs\n", argc - 1);
	return 0;
}
