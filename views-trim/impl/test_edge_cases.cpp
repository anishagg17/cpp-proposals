// test_edge_cases.cpp
//
// Extra checks for P4398R0, written to tie each claim in the proposed
// wording to observable behaviour of the reference implementation:
// boundary cases, const element access, constexpr, borrowed_range,
// iterator concepts, return types, the explicit constructor, and the
// complexity / caching claims (counted predicate calls).

#include "views_trim.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <deque>
#include <list>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sr = std::ranges;
namespace spv = std_proposal::views;
using std_proposal::ranges::trim_view;

using is_zero = decltype([](int i) { return i == 0; });

template <class R>
std::vector<sr::range_value_t<R>> to_vec(R&& r) {
    std::vector<sr::range_value_t<R>> out;
    for (auto&& e : r) out.push_back(e);
    return out;
}

// ---- wording claims checked at compile time ------------------------------

// [range.trim.view]: begin()/end() return iterator_t<V>.
using V = sr::ref_view<std::vector<int>>;
using TV = trim_view<V, is_zero>;
static_assert(std::same_as<decltype(std::declval<TV&>().begin()), sr::iterator_t<V>>);
static_assert(std::same_as<decltype(std::declval<TV&>().end()), sr::iterator_t<V>>);
static_assert(std::same_as<decltype(std::declval<const TV&>().pred()), const is_zero&>);
static_assert(std::same_as<decltype(std::declval<TV&&>().base()), V>);

// The constructor is explicit: no copy-list-initialisation from {base, pred}.
template <class T, class... A>
concept implicitly_list_constructible = requires(A... a) {
    [](T) {}({a...});
};
static_assert(std::constructible_from<TV, V, is_zero>);
static_assert(!implicitly_list_constructible<TV, V, is_zero>);

// Deduction guide: trim_view(R&&, Pred) -> trim_view<views::all_t<R>, Pred>.
static_assert(std::same_as<decltype(trim_view(std::declval<std::vector<int>&>(), is_zero{})),
                           trim_view<sr::ref_view<std::vector<int>>, is_zero>>);

// views::trim_while(E, P) and E | views::trim_while(P) give the same type.
static_assert(std::same_as<decltype(spv::trim_while(std::declval<std::vector<int>&>(), is_zero{})),
                           decltype(std::declval<std::vector<int>&>() | spv::trim_while(is_zero{}))>);

// Iterator concept is the base's own (vector: contiguous, deque: random
// access, list: bidirectional), and the view is always common.
static_assert(sr::contiguous_range<TV> && sr::common_range<TV>);
using DQ = decltype(std::declval<std::deque<int>&>() | spv::trim(0));
static_assert(sr::random_access_range<DQ> && !sr::contiguous_range<DQ> && sr::common_range<DQ>);
using LS = decltype(std::declval<std::list<int>&>() | spv::trim(0));
static_assert(sr::bidirectional_range<LS> && !sr::random_access_range<LS> && sr::common_range<LS>);

// A const trim_view is not a range (no const begin/end, by design).
static_assert(!sr::range<const TV>);

// const element access through a const base works.
using CV = decltype(std::declval<const std::vector<int>&>() | spv::trim(0));
static_assert(sr::range<CV>);
static_assert(std::same_as<sr::range_reference_t<CV>, const int&>);

// borrowed_range follows the base.
using SV = decltype(std::declval<std::string_view>() | spv::trim(' '));
static_assert(sr::borrowed_range<SV> && sr::contiguous_range<SV>);
using OV = decltype(std::declval<std::vector<int>>() | spv::trim(0));
static_assert(!sr::borrowed_range<OV>);

// sized_range over a sized-sentinel base; not over list.
static_assert(sr::sized_range<TV> && !sr::sized_range<LS>);

// ---- constexpr -------------------------------------------------------------
constexpr bool constexpr_cases() {
    std::array<int, 0> e{};
    std::array a{0, 0, 0};
    std::array b{1, 2, 3};
    std::array c{5};
    std::array d{0, 4, 0, 5, 0};
    auto te = e | spv::trim(0);
    auto ta = a | spv::trim(0);
    auto tb = b | spv::trim(0);
    auto tc = c | spv::trim(0);
    auto td = d | spv::trim(0);
    return te.empty() && ta.empty() && sr::distance(tb) == 3 &&
           sr::distance(tc) == 1 && *tc.begin() == 5 &&
           sr::distance(td) == 3 && td.front() == 4 && td.back() == 5;
}
static_assert(constexpr_cases());

int main() {
    // empty, all match, none match, single kept, single trimmed
    {
        std::vector<int> empty, all{0, 0}, none{1, 2}, one{7}, gone{0};
        assert(to_vec(empty | spv::trim(0)).empty());
        assert(to_vec(all | spv::trim(0)).empty());
        assert((to_vec(none | spv::trim(0)) == std::vector{1, 2}));
        assert((to_vec(one | spv::trim(0)) == std::vector{7}));
        assert(to_vec(gone | spv::trim(0)).empty());
    }
    // interior matches are kept
    {
        std::vector<int> v{0, 1, 0, 2, 0};
        assert((to_vec(v | spv::trim(0)) == std::vector{1, 0, 2}));
    }
    // const base: elements are read as const
    {
        const std::vector<int> v{0, 3, 0};
        auto t = v | spv::trim(0);
        assert((to_vec(t) == std::vector{3}));
    }
    // string_view: result converts back to a string_view of the kept part
    {
        std::string_view s = "\t hi there \n";
        auto t = s | spv::trim_while([](char c) { return c == ' ' || c == '\t' || c == '\n'; });
        assert(std::string_view(t.begin(), t.end()) == "hi there");
    }
    // Complexity: begin() makes (prefix + 1) predicate calls when something
    // is kept, end() makes at most (suffix + 1); a second call makes none.
    {
        int calls = 0;
        auto counting = [&calls](int i) { ++calls; return i == 0; };
        std::vector<int> v{0, 0, 1, 2, 0, 0, 0};   // prefix 2, suffix 3
        auto t = v | spv::trim_while(counting);
        calls = 0; (void)t.begin(); assert(calls == 3);
        calls = 0; (void)t.end();   assert(calls == 4);
        calls = 0; (void)t.begin(); (void)t.end(); assert(calls == 0);  // cached
        // all-matching: begin() scans everything once; end() makes no calls
        std::vector<int> z{0, 0, 0};
        auto tz = z | spv::trim_while(counting);
        calls = 0; (void)tz.begin(); assert(calls == 3);
        calls = 0; (void)tz.end();   assert(calls == 0);
        // the trailing scan never passes begin(): interior elements untouched
        std::vector<int> w{0, 5, 6, 7, 0};
        auto tw = w | spv::trim_while(counting);
        (void)tw.begin();
        calls = 0; (void)tw.end(); assert(calls == 2);
    }
    // Copying a view does not copy the cache: the copy rescans its own base.
    {
        int calls = 0;
        auto counting = [&calls](int i) { ++calls; return i == 0; };
        std::vector<int> v{0, 1, 0};
        auto t = v | spv::trim_while(counting);
        (void)t.begin(); (void)t.end();
        auto u = t;
        calls = 0; (void)u.begin(); assert(calls == 2);
    }
    std::puts("edge cases: PASS");
    return 0;
}
