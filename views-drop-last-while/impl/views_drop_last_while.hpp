// views_drop_last_while.hpp
//
// Reference implementation for the WG21 proposal
//   "Add std::views::drop_last_while and std::views::take_last_while"
// (Tier 1 in the SG9 ranges plan, P2760R1; neither adaptor exists in
// range-v3 — P2760 annotates both "(not in range-v3)").
//
// Implemented in namespace std_proposal (NOT std) so it can be built
// and tested against today's compilers (C++20/23) on top of
// std::ranges. Nothing in this header modifies namespace std except
// specialisations of std::ranges::enable_borrowed_range for this
// header's own class templates, which the standard permits for
// program-defined types.
//
// Semantics (the paper's proposed design):
//   * views::drop_last_while(pred) yields the elements of the base
//     range with the maximal trailing run of elements satisfying
//     pred removed: the back-end mirror of views::drop_while.
//   * views::take_last_while(pred) yields exactly that maximal
//     trailing run, in the base range's original (forward) order:
//     the back-end mirror of views::take_while. The two views
//     partition the base range at one boundary position.
//   * pred means "the element belongs to the trailing run", exactly
//     as in drop_while / take_while.
//   * The base range must model bidirectional_range and
//     common_range. Bidirectional is what bounds the cost: the
//     boundary is found by one backward scan over the trailing run
//     only. There is deliberately no forward-only mode (a forward
//     implementation would have to traverse and buffer the whole
//     range — a different, owning adaptor; Apple's swift-algorithms
//     makes the same refusal for its BidirectionalCollection-only
//     trimmingSuffix(while:)/suffix(while:)). Common is needed
//     because the scan starts from an iterator to the end, which a
//     sentinel cannot provide; views::common is the existing remedy.
//   * The views' iterators ARE the base range's iterators: the
//     boundary is found once by the backward scan and cached (a
//     non-propagating cache, as in drop_while_view). Each view is
//     therefore always a common_range and preserves the base's
//     bidirectional / random-access / contiguous properties, and
//     propagates borrowed_range.
//   * sized_range: because both bounds are base iterators,
//     view_interface::size() (end - begin) is available and exact
//     exactly when the base's iterator/sentinel pair is a
//     sized_sentinel pair — the drop_while_view precedent, as in the
//     companion trim proposal.
//   * Predicate evaluation: the predicate is applied starting from
//     the LAST element, moving backwards, and stops at the first
//     element for which it is false. It is therefore applied at
//     most (length of the trailing run) + 1 times, in reverse order
//     of the elements — observable for stateful predicates, and
//     stated explicitly in the paper rather than left implicit.
//   * Construction of either view is O(1); the scan happens on the
//     first begin()/end() call that needs the boundary, at most
//     once, and afterwards the bounds are O(1). Everything is
//     constexpr to the same extent as the base range's operations.
//   * There are deliberately no const begin()/end(): the cached
//     boundary follows drop_while_view, which likewise provides only
//     non-const begin(). A const view object is not iterable.

#ifndef STD_PROPOSAL_VIEWS_DROP_LAST_WHILE_HPP
#define STD_PROPOSAL_VIEWS_DROP_LAST_WHILE_HPP

#include <concepts>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace std_proposal::ranges {

namespace detail_dlw {

// Non-propagating cache, modelled on the standard's exposition-only
// non-propagating-cache ([range.nonprop.cache], used by
// drop_while_view): copying or moving the enclosing view yields an
// EMPTY cache in the destination, and the move operations
// additionally empty the SOURCE. A cached iterator may point into
// the source view object itself (views::single stores its element
// inside the view) or into a range owned by the source view, so a
// cache surviving in either the copy or the moved-from original can
// dangle once the other object is destroyed. The cache is a pure
// memo of a deterministic scan, so an empty cache simply recomputes.
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
// movable-box (drop_while_view stores its predicate this way): a
// semiregular wrapper that is copy- and move-assignable whenever T
// is copy/move CONSTRUCTIBLE, even when T itself is not assignable —
// the case that matters is a capturing lambda, whose closure type
// has no assignment operator. Without it, the views would fail to
// model std::ranges::view for exactly the predicates users write
// most, and composition (piping the result onward) would break.
// Assignment is manual: optional's own assignment operators are
// deleted for non-assignable T, so that path destroys and
// reconstructs (emplace).
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
    constexpr movable_box& operator=(const movable_box& other) {
        // Self-assignment has to be a no-op: the reconstruct path
        // below would otherwise destroy the stored value and then
        // read from the destroyed object. (The standard's
        // movable-box guards against this the same way.)
        if (this == std::addressof(other))
            return *this;
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
        if (this == std::addressof(other))
            return *this;
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

// The shared boundary computation for both views: the first
// position of the maximal trailing run of rng whose elements
// satisfy pred — equivalently, one past the last position whose
// element does NOT satisfy pred. Scans backwards from the end,
// applying pred to the last element first and stopping at the first
// element for which pred is false; the scan never passes begin(rng)
// and its cost is O(trailing run length + 1) predicate
// applications. The predicate is taken by const reference and only
// ever called as const, matching the views' constraint
// (indirect_unary_predicate<const Pred, ...>).
template <class Rng, class P>
constexpr auto trailing_run_begin(Rng& rng, const P& pred) {
    auto first = std::ranges::begin(rng);
    auto last = std::ranges::end(rng);
    while (last != first) {
        // Decrement a copy directly rather than std::prev(last):
        // std::prev dispatches on the iterator's category tag, and
        // some views report a category weaker than the operations
        // their iterators actually support; std::prev with
        // distance -1 on an input-category iterator is UB, which
        // GCC diagnoses under -Werror. The bidirectional_range
        // constraint on the views guarantees -- is valid here.
        auto prev = last;
        --prev;
        if (!std::invoke(pred, *prev))
            break;
        last = prev;
    }
    return last;
}

// A range/predicate pair the adaptors can act on. Stating this once,
// on the adaptor objects themselves (not only on the view classes),
// keeps misuse a substitution failure in the immediate context
// instead of a hard error deep inside a view's instantiation.
template <class R, class Pred>
concept droppable_last_while =
    std::ranges::viewable_range<R>
    && std::ranges::bidirectional_range<std::ranges::views::all_t<R>>
    && std::ranges::common_range<std::ranges::views::all_t<R>>
    && std::indirect_unary_predicate<
           const Pred, std::ranges::iterator_t<std::ranges::views::all_t<R>>>;

// Adaptor-closure plumbing. Where the standard's
// std::ranges::range_adaptor_closure is available (C++23), the
// closure types derive from it — that base is what makes
// closure | closure composition work before either adaptor is
// applied to a range. In C++20 there is no such base; the free
// operator| overloads further down then provide range | closure
// only.
//
// The selection must be a preprocessor macro (a constexpr variable
// is invisible to #if). The threshold is 202202L, the
// __cpp_lib_ranges value P2387 (range_adaptor_closure) specifies
// and the value GCC 13 / Clang 18 actually report in C++23 mode.
#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 202202L
#define STD_PROPOSAL_DLW_HAS_CLOSURE_BASE 1
#else
#define STD_PROPOSAL_DLW_HAS_CLOSURE_BASE 0
#endif

#if STD_PROPOSAL_DLW_HAS_CLOSURE_BASE
template <class D>
using closure_base = std::ranges::range_adaptor_closure<D>;
#else
template <class D>
struct closure_base {};
#endif

template <class Pred, template <class, class> class View>
struct dlw_closure : closure_base<dlw_closure<Pred, View>> {
    Pred pred_;

    constexpr explicit dlw_closure(Pred p) : pred_(std::move(p)) {}

    template <class R>
        requires droppable_last_while<R, Pred>
    constexpr auto operator()(R&& r) && {
        return View<std::ranges::views::all_t<R>, Pred>(
            std::ranges::views::all(std::forward<R>(r)), std::move(pred_));
    }
    template <class R>
        requires droppable_last_while<R, Pred>
    constexpr auto operator()(R&& r) const& {
        return View<std::ranges::views::all_t<R>, Pred>(
            std::ranges::views::all(std::forward<R>(r)), pred_);
    }
};

#if !STD_PROPOSAL_DLW_HAS_CLOSURE_BASE
template <class R, class Pred, template <class, class> class View>
    requires droppable_last_while<R, Pred>
constexpr auto operator|(R&& r, dlw_closure<Pred, View> c) {
    return std::move(c)(std::forward<R>(r));
}
#endif

} // namespace detail_dlw

// drop_last_while_view: see the file header for the design. V is a
// view (the adaptors below pass the input through views::all).
template <class V, class Pred>
    requires std::ranges::view<V> && std::ranges::bidirectional_range<V>
        && std::ranges::common_range<V> && std::is_object_v<Pred>
        && std::indirect_unary_predicate<const Pred,
                                         std::ranges::iterator_t<V>>
class drop_last_while_view
    : public std::ranges::view_interface<drop_last_while_view<V, Pred>> {
private:
    V base_ = V();
    detail_dlw::movable_box<Pred> pred_;
    // Cache for the end bound (the shared boundary). begin() needs
    // no scan: it is the base's begin.
    detail_dlw::non_propagating_cache<std::ranges::iterator_t<V>> end_cache_;

public:
    // Constrained exactly as the standard constrains the scanner
    // adaptors' default constructors (cf. drop_while_view): without
    // the requires-clause, checking default_initializable for a
    // non-default-initializable base such as ref_view becomes a
    // hard error inside Clang's C++23 adaptor machinery instead of
    // a clean "false".
    drop_last_while_view()
        requires std::default_initializable<V>
            && std::default_initializable<Pred>
    = default;

    constexpr drop_last_while_view(V base, Pred pred)
        : base_(std::move(base)), pred_(std::move(pred)) {}

    constexpr V base() const&
        requires std::copy_constructible<V>
    {
        return base_;
    }
    constexpr V base() && { return std::move(base_); }

    constexpr const Pred& pred() const noexcept { return *pred_; }

    constexpr auto begin() { return std::ranges::begin(base_); }

    constexpr auto end() {
        if (!end_cache_)
            end_cache_ = detail_dlw::trailing_run_begin(base_, *pred_);
        return *end_cache_;
    }

    // Deliberately no const begin()/end(): see the file header.
};

template <class R, class Pred>
drop_last_while_view(R&&, Pred)
    -> drop_last_while_view<std::ranges::views::all_t<R>, Pred>;

// take_last_while_view: the dual. end() is the base's end; begin()
// is the cached boundary, so iteration yields the trailing run in
// the base's original forward order.
template <class V, class Pred>
    requires std::ranges::view<V> && std::ranges::bidirectional_range<V>
        && std::ranges::common_range<V> && std::is_object_v<Pred>
        && std::indirect_unary_predicate<const Pred,
                                         std::ranges::iterator_t<V>>
class take_last_while_view
    : public std::ranges::view_interface<take_last_while_view<V, Pred>> {
private:
    V base_ = V();
    detail_dlw::movable_box<Pred> pred_;
    detail_dlw::non_propagating_cache<std::ranges::iterator_t<V>>
        begin_cache_;

public:
    take_last_while_view()
        requires std::default_initializable<V>
            && std::default_initializable<Pred>
    = default;

    constexpr take_last_while_view(V base, Pred pred)
        : base_(std::move(base)), pred_(std::move(pred)) {}

    constexpr V base() const&
        requires std::copy_constructible<V>
    {
        return base_;
    }
    constexpr V base() && { return std::move(base_); }

    constexpr const Pred& pred() const noexcept { return *pred_; }

    constexpr auto begin() {
        if (!begin_cache_)
            begin_cache_ = detail_dlw::trailing_run_begin(base_, *pred_);
        return *begin_cache_;
    }

    constexpr auto end() { return std::ranges::end(base_); }

    // Deliberately no const begin()/end(): see the file header.
};

template <class R, class Pred>
take_last_while_view(R&&, Pred)
    -> take_last_while_view<std::ranges::views::all_t<R>, Pred>;

namespace detail_dlw {

// The closure types name the view class templates, so the aliases
// live here, after them (dlw_closure itself is only instantiated
// through these aliases and the adaptor objects below).
template <class Pred>
using drop_last_while_closure = dlw_closure<Pred, drop_last_while_view>;
template <class Pred>
using take_last_while_closure = dlw_closure<Pred, take_last_while_view>;

} // namespace detail_dlw

namespace views {

struct drop_last_while_fn {
    // Two-argument form: views::drop_last_while(rng, pred)
    template <class R, class Pred>
        requires detail_dlw::droppable_last_while<R, std::decay_t<Pred>>
    constexpr auto operator()(R&& r, Pred&& pred) const {
        return drop_last_while_view<std::ranges::views::all_t<R>,
                                    std::decay_t<Pred>>(
            std::ranges::views::all(std::forward<R>(r)),
            std::forward<Pred>(pred));
    }

    // Pipe form: rng | views::drop_last_while(pred)
    template <class Pred>
    constexpr auto operator()(Pred&& pred) const {
        return detail_dlw::drop_last_while_closure<std::decay_t<Pred>>(
            std::forward<Pred>(pred));
    }
};

struct take_last_while_fn {
    // Two-argument form: views::take_last_while(rng, pred)
    template <class R, class Pred>
        requires detail_dlw::droppable_last_while<R, std::decay_t<Pred>>
    constexpr auto operator()(R&& r, Pred&& pred) const {
        return take_last_while_view<std::ranges::views::all_t<R>,
                                    std::decay_t<Pred>>(
            std::ranges::views::all(std::forward<R>(r)),
            std::forward<Pred>(pred));
    }

    // Pipe form: rng | views::take_last_while(pred)
    template <class Pred>
    constexpr auto operator()(Pred&& pred) const {
        return detail_dlw::take_last_while_closure<std::decay_t<Pred>>(
            std::forward<Pred>(pred));
    }
};

inline constexpr drop_last_while_fn drop_last_while{};
inline constexpr take_last_while_fn take_last_while{};

} // namespace views
} // namespace std_proposal::ranges

namespace std_proposal {
// Mirror std's own alias (std::views is std::ranges::views).
namespace views = ranges::views;
} // namespace std_proposal

namespace std::ranges {
// Both views are borrowed_ranges exactly when their base is (the
// computed boundary is an iterator into the base; nothing is owned
// beyond the view of the base itself).
template <class V, class Pred>
inline constexpr bool enable_borrowed_range<
    std_proposal::ranges::drop_last_while_view<V, Pred>> =
    enable_borrowed_range<V>;

template <class V, class Pred>
inline constexpr bool enable_borrowed_range<
    std_proposal::ranges::take_last_while_view<V, Pred>> =
    enable_borrowed_range<V>;

} // namespace std::ranges

#endif // STD_PROPOSAL_VIEWS_DROP_LAST_WHILE_HPP
