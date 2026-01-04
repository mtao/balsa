#if !defined(BALSA_UTILS_FACTORIAL_HPP)
#define BALSA_UTILS_FACTORIAL_HPP
#include <utility>

namespace balsa::utils {
namespace detail {
    template<int... N>
    int integer_sequence_product(std::integer_sequence<int, N...>) {
        return (N * ... * 1);
    }
}// namespace detail
template<int N>
constexpr int factorial() {
    return detail::integer_sequence_product(std::make_integer_sequence<int, N>());
}
constexpr int factorial(int N) { return (N > 1) ? (N * factorial(N - 1)) : 1; }
}// namespace balsa::utils
#endif
