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
