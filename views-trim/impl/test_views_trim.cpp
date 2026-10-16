// test_views_trim.cpp
//
// Tests for the reference implementation in views_trim.hpp.
// Compile-time properties are checked with static_assert / concepts;
// runtime behaviour with assert. Prints PASS at the end if everything
// succeeded.

#include "views_trim.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <forward_list>
#include <functional>
#include <iterator>
#include <list>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace spv = std_proposal::views;
namespace sr = std::ranges;

using is_zero = decltype([](int i) { return i == 0; });
using lt4 = decltype([](int i) { return i < 4; });

// ---- concept-level properties over a contiguous base ----------------------
using arr_view_t = decltype(std::declval<std::array<int, 7>&>() |
                            spv::trim_while(is_zero{}));
static_assert(sr::view<arr_view_t>);
static_assert(sr::common_range<arr_view_t>);
static_assert(sr::bidirectional_range<arr_view_t>);
static_assert(sr::random_access_range<arr_view_t>);
static_assert(sr::contiguous_range<arr_view_t>);
// sized_range follows the base's sized-sentinel pair (drop_while_view
// precedent): array/vector bases qualify, via view_interface::size().
static_assert(sr::sized_range<arr_view_t>);

// borrowed_range propagates from a borrowed base (span).
using span_view_t = decltype(std::declval<std::span<int>>() |
                             spv::trim_while(is_zero{}));
static_assert(sr::borrowed_range<span_view_t>);
static_assert(sr::common_range<span_view_t>);
// An lvalue vector base is a ref_view, which is itself borrowed, so a
// trim view over it is borrowed too.
using vec_view_t = decltype(std::declval<std::vector<int>&>() |
                            spv::trim_while(is_zero{}));
static_assert(sr::borrowed_range<vec_view_t>);
static_assert(sr::random_access_range<vec_view_t>);
// ... but an owning base (rvalue vector -> owning_view) is not borrowed.
using owning_view_t = decltype(std::declval<std::vector<int>>() |
                               spv::trim_while(is_zero{}));
static_assert(!sr::borrowed_range<owning_view_t>);

// list base: bidirectional preserved, random access not claimed, and
// no sized_range (list iterators are not a sized-sentinel pair).
using list_view_t = decltype(std::declval<std::list<int>&>() |
                             spv::trim_while(is_zero{}));
static_assert(sr::bidirectional_range<list_view_t>);
static_assert(!sr::random_access_range<list_view_t>);
static_assert(!sr::sized_range<list_view_t>);

// ---- rejected inputs (constraint checks must be substitution failures) ----
template <class R, class P>
concept trim_while_applicable = requires(R&& r, P&& p) {
    spv::trim_while(std::forward<R>(r), std::forward<P>(p));
};
template <class R, class T>
concept trim_applicable = requires(R&& r, T&& t) {
    spv::trim(std::forward<R>(r), std::forward<T>(t));
};

// no const begin()/end() on trim_view (drop_while_view parity)
template <class V, class P>
concept const_iterable_trim = requires(
    const std_proposal::ranges::trim_view<V, P>& v) { v.begin(); v.end(); };
static_assert(!const_iterable_trim<sr::ref_view<std::vector<int>>, is_zero>);

// views::trim(value) must reject element types with no == against the
// value — cleanly, via the predicate's requires-clause
struct no_eq {
    int v;
};
static_assert(!trim_applicable<std::vector<no_eq>&, int>);

// a predicate whose call operator is not const-callable is rejected
// (drop_while parity: the constraint is on const Pred)
struct nonconst_pred {
    bool operator()(int) { return true; }
};
static_assert(!trim_while_applicable<std::vector<int>&, nonconst_pred>);

static_assert(trim_while_applicable<std::vector<int>&, is_zero>);
// forward-only range: rejected (use views::drop_while for the lead edge)
static_assert(!trim_while_applicable<std::forward_list<int>&, is_zero>);
static_assert(!trim_applicable<std::forward_list<int>&, int>);
// bidirectional but NOT common (counted iterator + default sentinel)
using noncommon_t = sr::subrange<std::counted_iterator<std::vector<int>::iterator>,
                                 std::default_sentinel_t>;
static_assert(sr::bidirectional_range<noncommon_t>);
static_assert(!sr::common_range<noncommon_t>);
static_assert(!trim_while_applicable<noncommon_t, is_zero>);

// ---- constexpr evaluation ---------------------------------------------------
constexpr bool constexpr_nonconst_view() {
    std::array a{0, 0, 1, 2, 3, 0, 0};
    auto t = a | spv::trim(0);
    const int expected[]{1, 2, 3};
    int i = 0;
    for (int x : t) {
        if (x != expected[i++])
            return false;
    }
    return i == 3 && t.front() == 1 && t.back() == 3;
}
static_assert(constexpr_nonconst_view());

// Namespace scope on purpose: a ref_view over a *local* constexpr array
// stored in a constexpr variable is rejected by GCC 13 even for the
// standard's own views::take (verified) — a toolchain quirk, not a
// property of this implementation.
constexpr std::array kConstArr{7, 0, 0, 4, 5, 0, 7};

constexpr bool constexpr_const_view() {
    // Mutable local on purpose: trim_view (like drop_while_view) has
    // no const begin()/end(), so the view object cannot itself be
    // constexpr — constexpr EVALUATION still applies.
    auto t = kConstArr |
             spv::trim_while([](int i) { return i == 0 || i == 7; });
    const int expected[]{4, 5};
    int i = 0;
    for (int x : t) {
        if (x != expected[i++])
            return false;
    }
    return i == 2;
}
static_assert(constexpr_const_view());

// ---- helpers ----------------------------------------------------------------
template <class R>
auto to_vec(R&& r) {
    std::vector<std::ranges::range_value_t<R>> out;
    for (auto&& e : r)
        out.push_back(e);
    return out;
}

int main() {
    // empty range
    {
        std::vector<int> v;
        assert(to_vec(v | spv::trim_while(is_zero{})).empty());
        assert(to_vec(v | spv::trim(0)).empty());
    }
    // all elements trimmed
    {
        std::vector<int> v{0, 0, 0, 0};
        assert(to_vec(v | spv::trim_while(is_zero{})).empty());
        assert(to_vec(v | spv::trim(0)).empty());
    }
    // no elements trimmed
    {
        std::vector<int> v{1, 2, 3};
        assert((to_vec(v | spv::trim_while(is_zero{})) == std::vector{1, 2, 3}));
    }
    // leading only
    {
        std::vector<int> v{0, 0, 1, 2, 3};
        assert((to_vec(v | spv::trim_while(is_zero{})) == std::vector{1, 2, 3}));
    }
    // trailing only
    {
        std::vector<int> v{1, 2, 3, 0, 0};
        assert((to_vec(v | spv::trim_while(is_zero{})) == std::vector{1, 2, 3}));
    }
    // single element: kept, and trimmed
    {
        std::vector<int> kept{5};
        std::vector<int> gone{0};
        assert((to_vec(kept | spv::trim_while(is_zero{})) == std::vector{5}));
        assert(to_vec(gone | spv::trim_while(is_zero{})).empty());
    }
    // the range-v3 test case: pred i<4 over the classic array
    {
        int ia[] = {0, 1, 2, 3, 4, 3, 2, 1, 2, 3, 4, 3, 2, 1, 0};
        const std::vector expected{4, 3, 2, 1, 2, 3, 4};
        assert((to_vec(ia | spv::trim_while(lt4{})) == expected));
        assert((to_vec(sr::views::all(ia) | spv::trim_while(lt4{})) == expected));
        // two-argument form agrees with the pipe form
        assert((to_vec(spv::trim_while(ia, lt4{})) == expected));
    }
    // views::trim(value): the value companion (remove/remove_if pattern)
    {
        std::vector<int> v{0, 0, 5, 6, 0};
        assert((to_vec(v | spv::trim(0)) == std::vector{5, 6}));
        assert((to_vec(spv::trim(v, 0)) == std::vector{5, 6}));
        std::vector<int> w{9, 9, 9};
        assert(to_vec(w | spv::trim(9)).empty());
    }
    // strings: the motivating use case
    {
        std::string s = "  hello  ";
        auto t = s | spv::trim(' ');
        assert(std::string(t.begin(), t.end()) == "hello");
        auto is_space = [](char c) { return c == ' ' || c == '\t'; };
        std::string s2 = "\t world \t ";
        auto t2 = s2 | spv::trim_while(is_space);
        assert(std::string(t2.begin(), t2.end()) == "world");
        assert(std::string_view(t2.begin(), t2.end()) == "world");
    }
    // bidirectional but not random-access base
    {
        std::list<int> l{0, 1, 2, 0};
        assert((to_vec(l | spv::trim_while(is_zero{})) == std::vector{1, 2}));
    }
    // backward iteration through the view
    {
        std::vector<int> v{0, 1, 2, 3, 0};
        auto t = v | spv::trim_while(is_zero{});
        assert((to_vec(t | sr::views::reverse) == std::vector{3, 2, 1}));
    }
    // composition: trim AFTER transform
    {
        std::vector<int> v{0, 1, 2, 3, 0};
        auto t = v | sr::views::transform([](int i) { return i * 10; })
                 | spv::trim_while(is_zero{});
        assert((to_vec(t) == std::vector{10, 20, 30}));
    }
    // composition: trim BEFORE transform, and trim after filter
    {
        std::vector<int> v{0, 2, 4, 6, 0};
        auto t = v | spv::trim_while(is_zero{})
                 | sr::views::transform([](int i) { return i / 2; });
        assert((to_vec(t) == std::vector{1, 2, 3}));
        auto u = v | sr::views::filter([](int i) { return i != 4; })
                 | spv::trim_while(is_zero{});
        assert((to_vec(u) == std::vector{2, 6}));
    }
    // view_interface extras on a random-access base: [], front, back,
    // and ranges::size giving the exact trimmed size
    {
        std::array a{0, 10, 20, 30, 0};
        auto t = a | spv::trim_while(is_zero{});
        assert(t.front() == 10);
        assert(t.back() == 30);
        assert(t[1] == 20);
        assert(t.end() - t.begin() == 3);
        assert(sr::size(t) == 3);
        assert(!t.empty());
    }
    // pred() accessor round-trips the predicate
    {
        std::vector<int> v{1};
        auto t = v | spv::trim_while(lt4{});
        assert(t.pred()(3) && !t.pred()(4));
    }
    // (No const-iteration test: trim_view deliberately has no const
    // begin()/end() — asserted at namespace scope below main's
    // helpers; see const_iterable_trim.)
    // borrowed: a trim view over a span can escape its scope safely
    {
        std::array storage{0, 3, 1, 4, 0};
        auto make = [&] {
            return std::span<int>(storage) | spv::trim_while(is_zero{});
        };
        auto t = make();
        assert((to_vec(t) == std::vector{3, 1, 4}));
    }

    // Non-propagating cache (regression test, added 13 Oct 2026 after
    // external review): a cached iterator of a view over views::single
    // points INTO the view object, because single_view stores its
    // element internally. Copying the view must not copy the cache —
    // with a propagating cache the copy's begin() would dereference an
    // iterator into the destroyed original (use-after-free; this test
    // is also run under AddressSanitizer).
    {
        auto is_zero = [](int x) { return x == 0; };
        using view_t = decltype(std::views::single(7) |
                                spv::trim_while(is_zero));
        std::optional<view_t> copy;
        auto* original = new view_t(std::views::single(7) |
                                    spv::trim_while(is_zero));
        assert(*original->begin() == 7); // populate the begin cache
        copy = *original;                // copy the view
        delete original;                 // destroy the source view
        assert(copy.has_value());
        assert(*copy->begin() == 7);     // recomputed on the copy's base
        assert((to_vec(*copy) == std::vector{7}));
    }

    // Non-propagating cache, MOVE direction (regression test, review
    // round 2): the cache's move operations must also empty the
    // SOURCE. Here the view owns its base; after a move, the elements
    // live in the destination. If the moved-from view kept its cached
    // iterators, iterating it after the destination dies would
    // dereference freed storage (checked under AddressSanitizer).
    {
        auto is_zero = [](int x) { return x == 0; };
        auto tv = std_proposal::ranges::trim_view(
            std::ranges::owning_view(std::vector<int>{0, 0, 7, 8, 0}),
            is_zero);
        assert(*tv.begin() == 7); // populate the begin cache
        auto moved = std::move(tv);
        assert((to_vec(moved) == std::vector{7, 8}));
        {
            auto scoped = std::move(moved); // takes ownership away
            assert((to_vec(scoped) == std::vector{7, 8}));
        } // scoped (and the vector) destroyed here
        // tv's cache was emptied by the move and its base is
        // moved-from (empty): iterating tv is safe and yields nothing.
        assert(to_vec(tv).empty());
    }

    // Movable-box (regression test, review round 2): a capturing
    // lambda's closure type is not assignable, so without the box the
    // resulting trim_view would not model std::ranges::view and could
    // not be piped into further adaptors.
    {
        char c = ' ';
        auto lam = [c](char x) { return x == c; };
        std::string s = " hi ";
        auto v = s | spv::trim_while(lam);
        static_assert(sr::view<std::decay_t<decltype(v)>>);
        assert(std::string(v.begin(), v.end()) == "hi");
        // copy- and move-assignable despite the lambda predicate
        auto v2 = s | spv::trim_while(lam);
        v2 = v;
        assert(std::string(v2.begin(), v2.end()) == "hi");
        auto v3 = s | spv::trim_while(lam);
        v3 = std::move(v);
        assert(std::string(v3.begin(), v3.end()) == "hi");
        // composition after a lambda-predicated trim
        auto rev = s | spv::trim_while(lam) | sr::views::reverse;
        assert(std::string(rev.begin(), rev.end()) == "ih");
    }

    // Movable-box self-assignment (regression test): assigning a
    // view to itself must leave the stored predicate intact. The
    // predicate is a lambda capturing a heap-allocated string, so
    // its closure type is not assignable and the box stores it by
    // destroying and reconstructing on assignment; without a
    // self-assignment guard that path reads from the object it just
    // destroyed (use-after-free — this test also runs under
    // AddressSanitizer). The predicate compares against the captured
    // string's first character, so a corrupted capture changes the
    // result.
    {
        std::string target(64, 'x'); // forces a heap allocation (no SSO)
        auto is_x = [target](char ch) { return ch == target.front(); };
        std::string s = "xxhixx";
        auto v = s | spv::trim_while(is_x);
        assert(std::string(v.begin(), v.end()) == "hi");
        auto& vref = v;
        v = vref; // self copy-assignment
        assert(std::string(v.begin(), v.end()) == "hi");
        v = std::move(vref); // self move-assignment
        assert(std::string(v.begin(), v.end()) == "hi");
    }

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 202202L
    // C++23: adaptor closures compose before application, via
    // std::ranges::range_adaptor_closure. The static_assert is the
    // tripwire: if the header's closure-base path ever silently
    // disables itself again, this fails to compile.
    static_assert(std::derived_from<
                  decltype(spv::trim(' ')),
                  std::ranges::range_adaptor_closure<decltype(spv::trim(' '))>>);
    {
        auto composed = spv::trim(' ') | sr::views::reverse;
        std::string s = " ab ";
        auto applied = s | composed;
        assert(std::string(applied.begin(), applied.end()) == "ba");
    }
#endif

    std::puts("PASS");
    return 0;
}
