// P4398R0 reference implementation + edge-case tests (single file for Compiler Explorer)
// test_edge_cases.cpp
//
// Extra checks for P4398R0, written to tie each claim in the proposed
// wording to observable behaviour of the reference implementation:
// boundary cases, const element access, constexpr, borrowed_range,
// iterator concepts, return types, the explicit constructor, and the
// complexity / caching claims (counted predicate calls).

// ---- views_trim.hpp (inlined) ----
// views_trim.hpp
//
// Reference implementation for the WG21 proposal
//   "Add std::views::trim and std::views::trim_while"
// (The predicate form was called trim_if in the first draft and
// in the std-proposals float; it was renamed to trim_while after
// feedback on the list. Nothing else changed.)
// (Tier 2 in the SG9 ranges plan, P2760R0; the adaptor exists today
// only in range-v3 as ranges::views::trim).
//
// Implemented in namespace std_proposal (NOT std) so it can be built
// and tested against today's compilers (C++20/23) on top of
// std::ranges. Nothing in this header modifies namespace std except
// one specialisation of a std::ranges variable template for this
// header's own class template (enable_borrowed_range), which the
// standard permits for program-defined types.
//
// Semantics (the paper's proposed design):
//   * views::trim_while(pred) is a range adaptor closure. The resulting
//     view exposes the elements of the underlying range with the
//     maximal leading run and the maximal trailing run of elements
//     for which pred(element) is true removed. pred means "trim this
//     element", exactly as in drop_while / remove_if and in range-v3.
//   * views::trim(value) is the value-based companion: it trims
//     elements equal to value from both ends. It is implemented on
//     top of trim_while.
//   * The base range must model bidirectional_range and common_range
//     (range-v3 imposes the same two requirements). A forward-only
//     range is rejected at compile time; trimming just the leading
//     edge of such a range is already available as
//     views::drop_while(pred).
//   * The view's iterators ARE the base range's iterators (the bounds
//     are computed by scanning and then cached), so the view models
//     common_range, preserves bidirectional / random-access /
//     contiguous iteration of the base, and propagates borrowed_range.
//   * sized_range: because the view's iterators are the base's own
//     iterators, view_interface::size() (trimmed end - trimmed begin)
//     is available — and always yields the correct trimmed size —
//     exactly when the base's iterator/sentinel pair is a
//     sized_sentinel pair. trim_view therefore models sized_range in
//     precisely those cases. This follows the standard's own
//     drop_while_view, whose iterators are likewise the base's and
//     which models sized_range over e.g. a vector base for the same
//     reason (verified against libstdc++). It diverges from range-v3,
//     whose trim_view disables sized_range outright; see the paper's
//     design section for the discussion. (An earlier draft of this
//     header specialised disable_sized_range to imitate range-v3;
//     that was removed after testing showed it cannot suppress the
//     size() inherited from view_interface in libstdc++ — and the
//     drop_while precedent says it should not.)
//   * Construction of the view is O(1); each bound is computed by a
//     single scan, at most once (the non-const begin()/end() cache
//     their results). Everything is constexpr to the same extent as
//     the base range's operations.

#ifndef STD_PROPOSAL_VIEWS_TRIM_HPP
#define STD_PROPOSAL_VIEWS_TRIM_HPP

#include <concepts>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace std_proposal::ranges {

namespace detail_trim {

// Non-propagating cache, modelled on the standard's exposition-only
// non-propagating-cache ([range.nonprop.cache], used by
// drop_while_view and others): copying or moving the enclosing view
// yields an EMPTY cache in the destination, never a copy of the
// cached iterator — and the move operations additionally empty the
// SOURCE (other.reset(), as in the standard). Both directions matter:
// a cached iterator may point into the source view object itself
// (views::single stores its element inside the view) or into a range
// owned by the source view, so a cache surviving in either the copy
// or the moved-from original can dangle once the other object is
// destroyed — a use-after-free. The cache is a pure memo of a
// deterministic scan, so an empty cache simply recomputes.
template <class It>
struct non_propagating_cache : std::optional<It> {
    constexpr non_propagating_cache() = default;
    constexpr non_propagating_cache(const non_propagating_cache&) noexcept
        : std::optional<It>() {}
    constexpr non_propagating_cache(non_propagating_cache&& other) noexcept
        : std::optional<It>() {
        other.reset();
    }
    constexpr non_propagating_cache&
    operator=(const non_propagating_cache&) noexcept {
        this->reset();
        return *this;
    }
    constexpr non_propagating_cache&
    operator=(non_propagating_cache&& other) noexcept {
        this->reset();
        if (this != &other)
            other.reset();
        return *this;
    }
    constexpr non_propagating_cache& operator=(const It& it) {
        std::optional<It>::operator=(it);
        return *this;
    }
    constexpr non_propagating_cache& operator=(It&& it) {
        std::optional<It>::operator=(std::move(it));
        return *this;
    }
};

// Movable box, modelled on the standard's exposition-only
// movable-box ([range.nonprop.cache] family; drop_while_view stores
// its predicate this way): a semiregular wrapper that is copy- and
// move-assignable whenever T is copy/move CONSTRUCTIBLE, even when T
// itself is not assignable — the case that matters is a capturing
// lambda, whose closure type has no assignment operator. Without it,
// trim_view<V, Pred> would fail to model std::ranges::view (which
// requires movability) for exactly the predicates users write most,
// and composition (e.g. piping the result into views::reverse) would
// break. The storage is a std::optional, but assignment is manual:
// optional's own assignment operators are deleted for non-assignable
// T, so the non-assignable path destroys and reconstructs.
template <class T>
struct movable_box {
    std::optional<T> box_;
    constexpr movable_box() {
        if constexpr (std::default_initializable<T>)
            box_.emplace();
    }
    constexpr movable_box(T v) : box_(std::move(v)) {}
    constexpr movable_box(const movable_box&) = default;
    constexpr movable_box(movable_box&&) = default;
    // NOTE: assignment never assigns the optionals themselves —
    // optional<T>'s assignment operators are deleted outright when T
    // is not assignable, which is the very case this box exists for.
    // The non-assignable path destroys and reconstructs (emplace).
    constexpr movable_box& operator=(const movable_box& other) {
        if (!other.box_)
            box_.reset();
        else if constexpr (std::assignable_from<T&, const T&>) {
            if (box_)
                *box_ = *other.box_;
            else
                box_.emplace(*other.box_);
        } else
            box_.emplace(*other.box_);
        return *this;
    }
    constexpr movable_box& operator=(movable_box&& other) {
        if (!other.box_)
            box_.reset();
        else if constexpr (std::assignable_from<T&, T>) {
            if (box_)
                *box_ = std::move(*other.box_);
            else
                box_.emplace(std::move(*other.box_));
        } else
            box_.emplace(std::move(*other.box_));
        return *this;
    }
    constexpr T& operator*() & noexcept { return *box_; }
    constexpr const T& operator*() const& noexcept { return *box_; }
    constexpr T&& operator*() && noexcept { return std::move(*box_); }
    constexpr const T* operator->() const noexcept { return box_.operator->(); }
    constexpr T* operator->() noexcept { return box_.operator->(); }
};

} // namespace detail_trim

// trim_view: see the file header for the design. V is a view (the
// adaptors below pass the input through std::ranges::views::all).
template <class V, class Pred>
    requires std::ranges::view<V> && std::ranges::bidirectional_range<V>
        && std::ranges::common_range<V> && std::is_object_v<Pred>
        && std::indirect_unary_predicate<const Pred,
                                         std::ranges::iterator_t<V>>
class trim_view : public std::ranges::view_interface<trim_view<V, Pred>> {
private:
    V base_ = V();
    detail_trim::movable_box<Pred> pred_;
    detail_trim::non_propagating_cache<std::ranges::iterator_t<V>>
        begin_cache_;
    detail_trim::non_propagating_cache<std::ranges::iterator_t<V>>
        end_cache_;

    // First position in rng whose element does NOT satisfy pred.
    template <class Rng, class P>
    static constexpr auto compute_begin(Rng& rng, P& pred) {
        auto it = std::ranges::begin(rng);
        const auto last = std::ranges::end(rng);
        while (it != last && std::invoke(pred, *it))
            ++it;
        return it;
    }

    // One past the last position in rng, at or after first, whose
    // element does NOT satisfy pred. Scans backwards from the end.
    template <class Rng, class P>
    static constexpr auto compute_end(Rng& rng, P& pred,
                                      std::ranges::iterator_t<Rng> first) {
        auto last = std::ranges::end(rng);
        while (last != first) {
            // Decrement a copy directly rather than std::prev(last):
            // std::prev dispatches on the iterator's category tag, and
            // some views (e.g. transform_view with a prvalue-returning
            // function) report a category weaker than the operations
            // their iterators actually support; std::prev on an
            // input-category iterator with distance -1 is UB, which
            // GCC diagnoses under -Werror. The bidirectional_range
            // constraint on trim_view guarantees -- is valid here.
            auto prev = last;
            --prev;
            if (!std::invoke(pred, *prev))
                break;
            last = prev;
        }
        return last;
    }

public:
    // Constrained exactly as the standard constrains the scanner
    // adaptors' default constructors (cf. drop_while_view): without
    // the requires-clause, checking default_initializable<trim_view>
    // for a non-default-initializable base such as ref_view becomes a
    // hard error inside Clang's C++23 adaptor machinery instead of a
    // clean "false".
    trim_view()
        requires std::default_initializable<V>
            && std::default_initializable<Pred>
    = default;

    // explicit, as in the proposed wording (and drop_while_view).
    constexpr explicit trim_view(V base, Pred pred)
        : base_(std::move(base)), pred_(std::move(pred)) {}

    constexpr V base() const&
        requires std::copy_constructible<V>
    {
        return base_;
    }
    constexpr V base() && { return std::move(base_); }

    constexpr const Pred& pred() const { return *pred_; }

    constexpr std::ranges::iterator_t<V> begin() {
        if (!begin_cache_)
            begin_cache_ = compute_begin(base_, *pred_);
        return *begin_cache_;
    }

    constexpr std::ranges::iterator_t<V> end() {
        if (!end_cache_)
            end_cache_ = compute_end(base_, *pred_, begin());
        return *end_cache_;
    }

    // Deliberately no const begin()/end(): the cached-bounds design
    // follows drop_while_view, which likewise provides only non-const
    // begin() — a const trim_view is not iterable.
};

template <class R, class Pred>
trim_view(R&&, Pred) -> trim_view<std::ranges::views::all_t<R>, Pred>;

namespace detail_trim {

// A range/predicate pair the adaptors can act on. Stating this once,
// on the adaptor objects themselves (not only on trim_view), keeps
// misuse a substitution failure in the immediate context instead of a
// hard error deep inside trim_view's instantiation.
template <class R, class Pred>
concept trimmable =
    std::ranges::viewable_range<R>
    && std::ranges::bidirectional_range<std::ranges::views::all_t<R>>
    && std::ranges::common_range<std::ranges::views::all_t<R>>
    && std::indirect_unary_predicate<
           const Pred, std::ranges::iterator_t<std::ranges::views::all_t<R>>>;

// Adaptor-closure plumbing. Where the standard's
// std::ranges::range_adaptor_closure is available (C++23), the
// closure types derive from it — that base is what makes
// closure | closure composition (views::trim(' ') | views::reverse)
// work before either adaptor is applied to a range. In C++20 there
// is no such base; the free operator| overloads further down then
// provide range | closure only.
//
// The selection must be a preprocessor macro (a constexpr variable
// is invisible to #if): an earlier version keyed the operator|
// guard on a constexpr bool, which the preprocessor always saw as
// true. The threshold is 202202L, the __cpp_lib_ranges value P2387
// (range_adaptor_closure) specifies and the value GCC 13 / Clang 18
// actually report in C++23 mode — checking for the final C++23
// value 202207L silently disabled this path on both toolchains.
#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 202202L
#define STD_PROPOSAL_TRIM_HAS_CLOSURE_BASE 1
#else
#define STD_PROPOSAL_TRIM_HAS_CLOSURE_BASE 0
#endif

#if STD_PROPOSAL_TRIM_HAS_CLOSURE_BASE
template <class D>
using closure_base = std::ranges::range_adaptor_closure<D>;
#else
template <class D>
struct closure_base {};
#endif

template <class Pred>
struct trim_while_closure : closure_base<trim_while_closure<Pred>> {
    Pred pred_;

    constexpr explicit trim_while_closure(Pred p) : pred_(std::move(p)) {}

    template <class R>
        requires trimmable<R, Pred>
    constexpr auto operator()(R&& r) && {
        return trim_view<std::ranges::views::all_t<R>, Pred>(
            std::ranges::views::all(std::forward<R>(r)), std::move(pred_));
    }
    template <class R>
        requires trimmable<R, Pred>
    constexpr auto operator()(R&& r) const& {
        return trim_view<std::ranges::views::all_t<R>, Pred>(
            std::ranges::views::all(std::forward<R>(r)), pred_);
    }
};

#if !STD_PROPOSAL_TRIM_HAS_CLOSURE_BASE
template <class R, class Pred>
    requires trimmable<R, Pred>
constexpr auto operator|(R&& r, trim_while_closure<Pred> c) {
    return std::move(c)(std::forward<R>(r));
}
#endif

// Predicate behind views::trim(value): "element equals value". The
// requires-clause is load-bearing: without it, the declared bool
// return type lets the predicate appear to satisfy
// indirect_unary_predicate for element types that have no == against
// T, and the failure surfaces as a hard error inside the library
// instead of a clean constraint rejection at the adaptor.
template <class T>
struct eq_value_pred {
    T value_;
    // Forwarding, as in the wording's trim-equal-to (and ranges::equal_to),
    // so element types whose == needs a non-const or rvalue operand work.
    // boolean-testable is approximated by convertible_to<bool>.
    template <class X>
    constexpr bool operator()(X&& x) const
        requires requires(const T& v) {
            { std::forward<X>(x) == v } -> std::convertible_to<bool>;
        }
    {
        return bool(std::forward<X>(x) == value_);
    }
};

template <class T>
struct trim_closure : closure_base<trim_closure<T>> {
    T value_;

    constexpr explicit trim_closure(T v) : value_(std::move(v)) {}

    template <class R>
        requires trimmable<R, eq_value_pred<T>>
    constexpr auto operator()(R&& r) && {
        return trim_view<std::ranges::views::all_t<R>, eq_value_pred<T>>(
            std::ranges::views::all(std::forward<R>(r)),
            eq_value_pred<T>{std::move(value_)});
    }
    template <class R>
        requires trimmable<R, eq_value_pred<T>>
    constexpr auto operator()(R&& r) const& {
        return trim_view<std::ranges::views::all_t<R>, eq_value_pred<T>>(
            std::ranges::views::all(std::forward<R>(r)),
            eq_value_pred<T>{value_});
    }
};

#if !STD_PROPOSAL_TRIM_HAS_CLOSURE_BASE
template <class R, class T>
    requires trimmable<R, eq_value_pred<T>>
constexpr auto operator|(R&& r, trim_closure<T> c) {
    return std::move(c)(std::forward<R>(r));
}
#endif

} // namespace detail_trim

namespace views {

struct trim_while_fn {
    // Two-argument form: views::trim_while(rng, pred)
    template <class R, class Pred>
        requires detail_trim::trimmable<R, std::decay_t<Pred>>
    constexpr auto operator()(R&& r, Pred&& pred) const {
        return trim_view<std::ranges::views::all_t<R>, std::decay_t<Pred>>(
            std::ranges::views::all(std::forward<R>(r)),
            std::forward<Pred>(pred));
    }

    // Pipe form: rng | views::trim_while(pred)
    template <class Pred>
    constexpr auto operator()(Pred&& pred) const {
        return detail_trim::trim_while_closure<std::decay_t<Pred>>(
            std::forward<Pred>(pred));
    }
};

struct trim_fn {
    // Two-argument form: views::trim(rng, value)
    template <class R, class T>
        requires detail_trim::trimmable<
            R, detail_trim::eq_value_pred<std::decay_t<T>>>
    constexpr auto operator()(R&& r, T&& value) const {
        using pred_t = detail_trim::eq_value_pred<std::decay_t<T>>;
        return trim_view<std::ranges::views::all_t<R>, pred_t>(
            std::ranges::views::all(std::forward<R>(r)),
            pred_t{std::forward<T>(value)});
    }

    // Pipe form: rng | views::trim(value)
    template <class T>
    constexpr auto operator()(T&& value) const {
        return detail_trim::trim_closure<std::decay_t<T>>(
            std::forward<T>(value));
    }
};

inline constexpr trim_while_fn trim_while{};
inline constexpr trim_fn trim{};

} // namespace views
} // namespace std_proposal::ranges

namespace std_proposal {
// Mirror std's own alias (std::views is std::ranges::views).
namespace views = ranges::views;
} // namespace std_proposal

namespace std::ranges {
// trim_view is a borrowed_range exactly when its base is (the computed
// bounds are iterators into the base; nothing is owned beyond the
// view of the base itself). Mirrors range-v3's specialisation.
template <class V, class Pred>
inline constexpr bool
    enable_borrowed_range<std_proposal::ranges::trim_view<V, Pred>> =
        enable_borrowed_range<V>;

} // namespace std::ranges

#endif // STD_PROPOSAL_VIEWS_TRIM_HPP

// ---- end of views_trim.hpp ----

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
