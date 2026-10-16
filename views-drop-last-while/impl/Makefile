CXXFLAGS_COMMON = -Wall -Wextra -Werror
# Use clang++ when it is on PATH; fall back to a versioned binary.
CLANGXX := $(shell command -v clang++ 2>/dev/null || command -v clang++-18 2>/dev/null)

test:
	g++        -std=c++20 $(CXXFLAGS_COMMON) test_views_drop_last_while.cpp -o test_g++_c++20        && ./test_g++_c++20
	g++        -std=c++23 $(CXXFLAGS_COMMON) test_views_drop_last_while.cpp -o test_g++_c++23        && ./test_g++_c++23
	$(CLANGXX) -std=c++20 $(CXXFLAGS_COMMON) test_views_drop_last_while.cpp -o test_clang++_c++20     && ./test_clang++_c++20
	$(CLANGXX) -std=c++23 $(CXXFLAGS_COMMON) test_views_drop_last_while.cpp -o test_clang++_c++23     && ./test_clang++_c++23

clean:
	rm -f test_g++_c++20 test_g++_c++23 test_clang++_c++20 test_clang++_c++23

.PHONY: test clean
