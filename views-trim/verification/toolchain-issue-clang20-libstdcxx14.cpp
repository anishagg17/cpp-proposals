#include <ranges>
// Reduced reproducer: a class-template closure deriving from
// std::ranges::range_adaptor_closure, composed with views::reverse.
// Fails on Clang 20 + libstdc++ 14 (-std=c++23); compiles on GCC 14,
// Clang 20 + libc++, and Clang 18 + libstdc++ 13. Nothing from P4398 here.
#include <string>
template<class T>
struct tc : std::ranges::range_adaptor_closure<tc<T>> {
  T v;
  constexpr explicit tc(T x) : v(x) {}
#ifdef REFQ
  template<class R> constexpr auto operator()(R&& r) && { return std::views::all(std::forward<R>(r)); }
  template<class R> constexpr auto operator()(R&& r) const& { return std::views::all(std::forward<R>(r)); }
#else
  template<class R> constexpr auto operator()(R&& r) const { return std::views::all(std::forward<R>(r)); }
#endif
};
int main(){
  std::string s="ab";
  auto mc = tc<char>(' ') | std::views::reverse;
  (void)std::move(mc)(s); (void)mc(s); (void)(s|mc);
}
