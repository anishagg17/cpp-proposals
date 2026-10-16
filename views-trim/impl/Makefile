CXXFLAGS_COMMON = -Wall -Wextra -Werror

test:
	g++     -std=c++20 $(CXXFLAGS_COMMON) test_views_trim.cpp -o test_g++_c++20     && ./test_g++_c++20
	g++     -std=c++23 $(CXXFLAGS_COMMON) test_views_trim.cpp -o test_g++_c++23     && ./test_g++_c++23
	clang++ -std=c++20 $(CXXFLAGS_COMMON) test_views_trim.cpp -o test_clang++_c++20 && ./test_clang++_c++20
	clang++ -std=c++23 $(CXXFLAGS_COMMON) test_views_trim.cpp -o test_clang++_c++23 && ./test_clang++_c++23

clean:
	rm -f test_g++_c++20 test_g++_c++23 test_clang++_c++20 test_clang++_c++23

.PHONY: test clean
