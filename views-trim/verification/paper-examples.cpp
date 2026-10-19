// Every code example in paper/P4398R0.md, compiled and checked against
// the reference implementation (std:: names mapped to std_proposal::).
#include "../impl/views_trim.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace pv = std_proposal::views;

std::string_view trim_ws_today(std::string_view s) {          // "today" example
    auto first = s.find_first_not_of(" \t\n\r");
    if (first == std::string_view::npos) return {};
    auto last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, last - first + 1);
}

int main() {
    // Abstract
    std::string_view s = "  hello  ";
    auto t = s | pv::trim(' ');
    assert(std::string_view(t.begin(), t.end()) == "hello");
    std::vector<int> v = {0, 0, 7, 8, 0};
    auto u = v | pv::trim_while([](int i) { return i == 0; });
    assert((std::vector<int>(u.begin(), u.end()) == std::vector{7, 8}));

    // Token example: proposed form, and agreement with the hand-written one
    auto is_ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    for (std::string_view in : {"  a b \n", "", "   ", "x", "\t\tx"}) {
        std::string_view trimmed(in | pv::trim_while(is_ws));
        assert(trimmed == trim_ws_today(in));
    }

    // Audio example
    std::vector<float> buf{0.f, 0.f, 0.5f, 0.f, 0.25f, 0.f};
    std::span<const float> samples(buf);
    auto kept = samples | pv::trim(0.0f);
    std::span<const float> body(kept.begin(), kept.end());
    assert(body.size() == 3 && body.front() == 0.5f && body.back() == 0.25f);
    int n = 0; for (float x : samples | pv::trim(0.0f)) { (void)x; ++n; } assert(n == 3);

    // Composing example
    std::string_view line = " a , bb,c  ";
    auto fields = line
                | std::views::split(',')
                | std::views::transform([](auto f) { return std::string_view(f | pv::trim(' ')); });
    std::vector<std::string_view> got(fields.begin(), fields.end());
    assert((got == std::vector<std::string_view>{"a", "bb", "c"}));

    // Wording Example 1
    std::string out;
    for (char c : s | pv::trim(' ')) out += c;
    assert(out == "hello");

    // "Why not compose": reverse|drop_while|reverse loses contiguity
    auto p = [](char c) { return c == ' '; };
    using Comp = decltype(s | std::views::reverse | std::views::drop_while(p) | std::views::reverse);
    static_assert(std::ranges::random_access_range<Comp> && !std::ranges::contiguous_range<Comp>);
    static_assert(std::ranges::contiguous_range<decltype(s | pv::trim(' '))>);

    std::puts("paper examples: PASS");
}
