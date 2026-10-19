// Checks the exposition-only trim-equal-to from the wording (with
// boolean-testable approximated by convertible_to<bool>).
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
template<class T>
struct trim_equal_to {
    T value;
    template<class U>
      requires requires(U&& u, const T& v) { { std::forward<U>(u) == v } -> std::convertible_to<bool>; }
    constexpr bool operator()(U&& u) const { return bool(std::forward<U>(u) == value); }
};
struct no_eq {};
// An element type whose == only works on a non-const operand: accepted by the
// forwarding form, rejected by a const U& form.
struct mut_eq { int v; bool operator==(int x) { return v == x; } };
static_assert(std::is_invocable_v<const trim_equal_to<int>, mut_eq&>);
static_assert(std::is_invocable_r_v<bool, const trim_equal_to<char>, const char&>);
static_assert(std::is_invocable_r_v<bool, const trim_equal_to<std::string>, std::string_view>);
static_assert(!std::is_invocable_v<const trim_equal_to<int>, const no_eq&>);
constexpr trim_equal_to<int> z{0};
static_assert(z(0) && !z(1));
int main() {}
