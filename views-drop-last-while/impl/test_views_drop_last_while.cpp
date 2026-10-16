// test_views_drop_last_while.cpp
//
// Tests for the reference implementation in views_drop_last_while.hpp.
// Compile-time properties are checked with static_assert / concepts;
// runtime behaviour with assert. Prints PASS at the end if everything
// succeeded.

#include "views_drop_last_while.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <forward_list>
#include <functional>
#include <iterator>
#include <list>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace spv = std_proposal::views;
namespace sr = std::ranges;

using is_zero = decltype([](int i) { return i == 0; });
using is_space = decltype([](char c) { return c == ' ' || c == '\t'; });

// ---- concept-level properties over a contiguous base ----------------------
using arr_drop_t = decltype(std::declval<std::array<int, 7>&>() |
                            spv::drop_last_while(is_zero{}));
using arr_take_t = decltype(std::declval<std::array<int, 7>&>() |
                            spv::take_last_while(is_zero{}));
static_assert(sr::view<arr_drop_t>);
static_assert(sr::view<arr_take_t>);
static_assert(sr::common_range<arr_drop_t>);
static_assert(sr::common_range<arr_take_t>);
static_assert(sr::bidirectional_range<arr_drop_t>);
static_assert(sr::bidirectional_range<arr_take_t>);
static_assert(sr::random_access_range<arr_drop_t>);
static_assert(sr::random_access_range<arr_take_t>);
static_assert(sr::contiguous_range<arr_drop_t>);
static_assert(sr::contiguous_range<arr_take_t>);
// sized_range follows the base's sized-sentinel pair (drop_while_view
// precedent): array/vector bases qualify, via view_interface::size().
static_assert(sr::sized_range<arr_drop_t>);
static_assert(sr::sized_range<arr_take_t>);

// borrowed_range propagates from a borrowed base (span).
using span_drop_t = decltype(std::declval<std::span<int>>() |
                             spv::drop_last_while(is_zero{}));
using span_take_t = decltype(std::declval<std::span<int>>() |
                             spv::take_last_while(is_zero{}));
static_assert(sr::borrowed_range<span_drop_t>);
static_assert(sr::borrowed_range<span_take_t>);
// An lvalue vector base is a ref_view, which is itself borrowed.
using vec_drop_t = decltype(std::declval<std::vector<int>&>() |
                            spv::drop_last_while(is_zero{}));
static_assert(sr::borrowed_range<vec_drop_t>);
// ... but an owning base (rvalue vector -> owning_view) is not borrowed.
using owning_drop_t = decltype(std::declval<std::vector<int>>() |
                               spv::drop_last_while(is_zero{}));
static_assert(!sr::borrowed_range<owning_drop_t>);

// list base: bidirectional preserved, random access not claimed, and
// no sized_range (list iterators are not a sized-sentinel pair).
using list_drop_t = decltype(std::declval<std::list<int>&>() |
                             spv::drop_last_while(is_zero{}));
using list_take_t = decltype(std::declval<std::list<int>&>() |
                             spv::take_last_while(is_zero{}));
static_assert(sr::bidirectional_range<list_drop_t>);
static_assert(sr::bidirectional_range<list_take_t>);
static_assert(!sr::random_access_range<list_drop_t>);
static_assert(!sr::sized_range<list_drop_t>);
static_assert(!sr::sized_range<list_take_t>);

// ---- rejected inputs (constraint checks must be substitution failures) ----
template <class R, class P>
concept drop_last_while_applicable = requires(R&& r, P&& p) {
    spv::drop_last_while(std::forward<R>(r), std::forward<P>(p));
};
template <class R, class P>
concept take_last_while_applicable = requires(R&& r, P&& p) {
    spv::take_last_while(std::forward<R>(r), std::forward<P>(p));
};

// no const begin()/end() on either view (drop_while_view parity)
template <class V>
concept const_iterable = requires(const V& v) { v.begin(); v.end(); };
static_assert(!const_iterable<
              std_proposal::ranges::drop_last_while_view<
                  sr::ref_view<std::vector<int>>, is_zero>>);
static_assert(!const_iterable<
              std_proposal::ranges::take_last_while_view<
                  sr::ref_view<std::vector<int>>, is_zero>>);

// a predicate whose call operator is not const-callable is rejected
// (drop_while parity: the constraint is on const Pred)
struct nonconst_pred {
    bool operator()(int) { return true; }
};
static_assert(!drop_last_while_applicable<std::vector<int>&, nonconst_pred>);
static_assert(!take_last_while_applicable<std::vector<int>&, nonconst_pred>);

static_assert(drop_last_while_applicable<std::vector<int>&, is_zero>);
static_assert(take_last_while_applicable<std::vector<int>&, is_zero>);
// forward-only range: rejected (no silently-inefficient forward mode)
static_assert(!drop_last_while_applicable<std::forward_list<int>&, is_zero>);
static_assert(!take_last_while_applicable<std::forward_list<int>&, is_zero>);
// bidirectional but NOT common (counted iterator + default sentinel)
using noncommon_t = sr::subrange<std::counted_iterator<std::vector<int>::iterator>,
                                 std::default_sentinel_t>;
static_assert(sr::bidirectional_range<noncommon_t>);
static_assert(!sr::common_range<noncommon_t>);
static_assert(!drop_last_while_applicable<noncommon_t, is_zero>);
static_assert(!take_last_while_applicable<noncommon_t, is_zero>);

// ---- constexpr evaluation ---------------------------------------------------
constexpr bool constexpr_views() {
    std::array a{1, 2, 3, 0, 0};
    auto d = a | spv::drop_last_while(is_zero{});
    auto t = a | spv::take_last_while(is_zero{});
    const int exp_d[]{1, 2, 3};
    const int exp_t[]{0, 0};
    int i = 0;
    for (int x : d) {
        if (x != exp_d[i++])
            return false;
    }
    if (i != 3)
        return false;
    i = 0;
    for (int x : t) {
        if (x != exp_t[i++])
            return false;
    }
    return i == 2 && d.front() == 1 && d.back() == 3 && t.front() == 0 &&
           t.back() == 0;
}
static_assert(constexpr_views());

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
        assert(to_vec(v | spv::drop_last_while(is_zero{})).empty());
        assert(to_vec(v | spv::take_last_while(is_zero{})).empty());
    }
    // all elements match: drop yields empty, take yields everything
    {
        std::vector<int> v{0, 0, 0, 0};
        assert(to_vec(v | spv::drop_last_while(is_zero{})).empty());
        assert((to_vec(v | spv::take_last_while(is_zero{})) ==
                std::vector{0, 0, 0, 0}));
    }
    // no elements match: drop yields everything, take yields empty
    {
        std::vector<int> v{1, 2, 3};
        assert((to_vec(v | spv::drop_last_while(is_zero{})) ==
                std::vector{1, 2, 3}));
        assert(to_vec(v | spv::take_last_while(is_zero{})).empty());
    }
    // partial trailing run; the leading run is untouched by both views
    {
        std::vector<int> v{0, 0, 1, 2, 0, 0};
        assert((to_vec(v | spv::drop_last_while(is_zero{})) ==
                std::vector{0, 0, 1, 2}));
        assert((to_vec(v | spv::take_last_while(is_zero{})) ==
                std::vector{0, 0}));
    }
    // single element: matching and non-matching
    {
        std::vector<int> m{0}, n{5};
        assert(to_vec(m | spv::drop_last_while(is_zero{})).empty());
        assert((to_vec(m | spv::take_last_while(is_zero{})) == std::vector{0}));
        assert((to_vec(n | spv::drop_last_while(is_zero{})) == std::vector{5}));
        assert(to_vec(n | spv::take_last_while(is_zero{})).empty());
    }
    // partition property: drop ++ take == the original sequence
    {
        std::vector<int> v{3, 1, 4, 0, 1, 0, 0};
        auto kept = to_vec(v | spv::drop_last_while(is_zero{}));
        auto run = to_vec(v | spv::take_last_while(is_zero{}));
        kept.insert(kept.end(), run.begin(), run.end());
        assert(kept == v);
    }
    // take_last_while preserves the ORIGINAL (forward) order — it is
    // not reverse | take_while: a non-palindromic trailing run proves it
    {
        std::vector<int> v{9, 1, 2, 3};
        auto small = [](int i) { return i < 5; };
        assert((to_vec(v | spv::take_last_while(small)) ==
                std::vector{1, 2, 3}));
        assert((to_vec(v | spv::drop_last_while(small)) == std::vector{9}));
    }
    // two-argument forms agree with the pipe forms
    {
        std::vector<int> v{1, 2, 0, 0};
        assert((to_vec(spv::drop_last_while(v, is_zero{})) ==
                std::vector{1, 2}));
        assert((to_vec(spv::take_last_while(v, is_zero{})) ==
                std::vector{0, 0}));
    }
    // strings: the motivating use case — trailing whitespace
    {
        std::string s = "hello  \t ";
        auto d = s | spv::drop_last_while(is_space{});
        assert(std::string(d.begin(), d.end()) == "hello");
        auto t = s | spv::take_last_while(is_space{});
        assert(std::string(t.begin(), t.end()) == "  \t ");
        // leading whitespace is NOT touched (that is drop_while's job)
        std::string s2 = "  hi";
        auto d2 = s2 | spv::drop_last_while(is_space{});
        assert(std::string(d2.begin(), d2.end()) == "  hi");
    }
    // the split-then-drop pattern from the Kotlin evidence: discard
    // the trailing empty fields a split leaves behind
    {
        std::vector<std::string> fields{"a", "b", "", ""};
        auto is_empty = [](const std::string& f) { return f.empty(); };
        auto d = fields | spv::drop_last_while(is_empty);
        assert((to_vec(d) == std::vector<std::string>{"a", "b"}));
    }
    // bidirectional but not random-access base
    {
        std::list<int> l{1, 2, 0, 0};
        assert((to_vec(l | spv::drop_last_while(is_zero{})) ==
                std::vector{1, 2}));
        assert((to_vec(l | spv::take_last_while(is_zero{})) ==
                std::vector{0, 0}));
    }
    // backward iteration through both views
    {
        std::vector<int> v{1, 2, 3, 0, 0};
        auto d = v | spv::drop_last_while(is_zero{});
        assert((to_vec(d | sr::views::reverse) == std::vector{3, 2, 1}));
        auto t = v | spv::take_last_while([](int i) { return i != 3; });
        assert((to_vec(t | sr::views::reverse) == std::vector{0, 0}));
    }
    // composition: after transform, and before transform / chained
    // with the front-side mirrors (drop_while + drop_last_while = trim)
    {
        std::vector<int> v{0, 1, 2, 0};
        auto t = v | sr::views::transform([](int i) { return i * 10; })
                 | spv::drop_last_while(is_zero{});
        // only the TRAILING zero goes; the leading one stays
        assert((to_vec(t) == std::vector{0, 10, 20}));
        auto u = v | spv::drop_last_while(is_zero{})
                 | sr::views::transform([](int i) { return i + 1; });
        assert((to_vec(u) == std::vector{1, 2, 3}));
        auto trimmed = v | sr::views::drop_while(is_zero{})
                       | spv::drop_last_while(is_zero{});
        assert((to_vec(trimmed) == std::vector{1, 2}));
    }
    // view_interface extras on a random-access base
    {
        std::array a{10, 20, 30, 0, 0};
        auto d = a | spv::drop_last_while(is_zero{});
        assert(d.front() == 10);
        assert(d.back() == 30);
        assert(d[1] == 20);
        assert(d.end() - d.begin() == 3);
        assert(sr::size(d) == 3);
        auto t = a | spv::take_last_while(is_zero{});
        assert(t.front() == 0 && t.back() == 0);
        assert(sr::size(t) == 2);
    }
    // pred() accessor round-trips the predicate
    {
        std::vector<int> v{1};
        auto d = v | spv::drop_last_while(is_zero{});
        assert(d.pred()(0) && !d.pred()(1));
        auto t = v | spv::take_last_while(is_zero{});
        assert(t.pred()(0) && !t.pred()(1));
    }
    // predicate evaluation: starts at the LAST element, moves
    // backwards, stops at the first non-match — (run length + 1)
    // applications, and the result is cached (a second begin()/end()
    // pass applies the predicate no further times)
    {
        std::vector<int> seen;
        auto recording = [&seen](int i) {
            seen.push_back(i);
            return i == 0;
        };
        std::vector<int> v{7, 2, 0, 0};
        auto d = v | spv::drop_last_while(recording);
        assert((to_vec(d) == std::vector{7, 2}));
        assert((seen == std::vector{0, 0, 2})); // last-to-first order
        seen.clear();
        auto d2 = v | spv::drop_last_while(recording);
        (void)d2.end();
        (void)d2.end(); // cached: no second scan
        assert((seen == std::vector{0, 0, 2}));
        seen.clear();
        auto t = v | spv::take_last_while(recording);
        assert((to_vec(t) == std::vector{0, 0}));
        assert((seen == std::vector{0, 0, 2}));
    }
    // borrowed: a view over a span can escape its scope safely
    {
        std::array storage{3, 1, 4, 0};
        auto make = [&] {
            return std::span<int>(storage) | spv::drop_last_while(is_zero{});
        };
        auto d = make();
        assert((to_vec(d) == std::vector{3, 1, 4}));
    }

    // Non-propagating cache (regression test, same shape as the trim
    // one): a cached iterator of a view over views::single points
    // INTO the view object. Copying the view must not copy the cache
    // — with a propagating cache the copy's begin() would dereference
    // an iterator into the destroyed original (use-after-free; this
    // test also runs under AddressSanitizer).
    {
        auto is_seven = [](int x) { return x == 7; };
        using view_t = decltype(std::views::single(7) |
                                spv::take_last_while(is_seven));
        std::optional<view_t> copy;
        auto* original = new view_t(std::views::single(7) |
                                    spv::take_last_while(is_seven));
        assert(*original->begin() == 7); // populate the begin cache
        copy = *original;                // copy the view
        delete original;                 // destroy the source view
        assert(copy.has_value());
        assert(*copy->begin() == 7);     // recomputed on the copy's base
        assert((to_vec(*copy) == std::vector{7}));
    }

    // Non-propagating cache, MOVE direction: the cache's move
    // operations must also empty the SOURCE. The view owns its base;
    // after a move the elements live in the destination. If the
    // moved-from view kept its cached boundary, iterating it after
    // the destination dies would dereference freed storage (checked
    // under AddressSanitizer).
    {
        auto dv = std_proposal::ranges::drop_last_while_view(
            std::ranges::owning_view(std::vector<int>{7, 8, 0, 0}),
            is_zero{});
        assert(dv.end() - dv.begin() == 2); // populates the end cache
        auto moved = std::move(dv);
        assert((to_vec(moved) == std::vector{7, 8}));
        {
            auto scoped = std::move(moved); // takes ownership away
            assert((to_vec(scoped) == std::vector{7, 8}));
        } // scoped (and the vector) destroyed here
        // dv's cache was emptied by the move and its base is
        // moved-from (empty): iterating dv is safe and yields nothing.
        assert(to_vec(dv).empty());
    }

    // Movable-box: a capturing lambda's closure type is not
    // assignable, so without the box the views would not model
    // std::ranges::view and could not be piped into further adaptors.
    {
        int zero = 0;
        auto lam = [zero](int x) { return x == zero; };
        std::vector<int> v{1, 2, 0, 0};
        auto d = v | spv::drop_last_while(lam);
        static_assert(sr::view<std::decay_t<decltype(d)>>);
        assert((to_vec(d) == std::vector{1, 2}));
        auto d2 = v | spv::drop_last_while(lam);
        d2 = d; // copy-assignable despite the lambda predicate
        assert((to_vec(d2) == std::vector{1, 2}));
        auto d3 = v | spv::drop_last_while(lam);
        d3 = std::move(d); // move-assignable
        assert((to_vec(d3) == std::vector{1, 2}));
        auto rev = v | spv::drop_last_while(lam) | sr::views::reverse;
        assert((to_vec(rev) == std::vector{2, 1}));
    }

    // Movable-box self-assignment (regression test): assigning a
    // view to itself must leave the stored predicate intact. The
    // predicate is a lambda capturing a heap-allocated string, so
    // its closure type is not assignable and the box stores it by
    // destroying and reconstructing on assignment; without a
    // self-assignment guard that path reads from the object it just
    // destroyed (use-after-free — this test also runs under
    // AddressSanitizer). The predicate reads the captured string on
    // every call, so a corrupted capture changes the result.
    {
        std::string big(64, 'x'); // forces a heap allocation (no SSO)
        auto pred = [big](int x) { return x == 0 && big.size() == 64; };
        std::vector<int> v{1, 2, 0, 0};
        auto d = v | spv::drop_last_while(pred);
        assert((to_vec(d) == std::vector{1, 2}));
        auto& dref = d;
        d = dref; // self copy-assignment
        assert((to_vec(d) == std::vector{1, 2}));
        d = std::move(dref); // self move-assignment
        assert((to_vec(d) == std::vector{1, 2}));
        auto t = v | spv::take_last_while(pred);
        assert((to_vec(t) == std::vector{0, 0}));
        auto& tref = t;
        t = tref; // self copy-assignment
        assert((to_vec(t) == std::vector{0, 0}));
        t = std::move(tref); // self move-assignment
        assert((to_vec(t) == std::vector{0, 0}));
    }

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 202202L
    // C++23: adaptor closures compose before application, via
    // std::ranges::range_adaptor_closure. The static_assert is the
    // tripwire: if the header's closure-base path ever silently
    // disables itself, this fails to compile.
    static_assert(std::derived_from<
                  decltype(spv::drop_last_while(is_zero{})),
                  std::ranges::range_adaptor_closure<
                      decltype(spv::drop_last_while(is_zero{}))>>);
    {
        auto composed = spv::drop_last_while(is_zero{}) | sr::views::reverse;
        std::vector<int> v{1, 2, 3, 0};
        auto applied = v | composed;
        assert((to_vec(applied) == std::vector{3, 2, 1}));
    }
#endif

    std::puts("PASS");
    return 0;
}
