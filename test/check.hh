// A minimal check macro: counts failures, prints the first few, keeps checks live under NDEBUG.
#pragma once

#include <cstdio>

namespace check {
inline int failures = 0;
}

#define CHECK(cond)                                                                          \
	do {                                                                                     \
		if (!(cond)) {                                                                       \
			if (++check::failures <= 20) std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		}                                                                                    \
	} while (0)

#define CHECK_MSG(cond, ...)                                                                 \
	do {                                                                                     \
		if (!(cond)) {                                                                       \
			if (++check::failures <= 20) {                                                   \
				std::printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond);                  \
				std::printf(__VA_ARGS__);                                                    \
				std::printf("\n");                                                           \
			}                                                                                \
		}                                                                                    \
	} while (0)
