#if !defined(BALSA_POLYNOMIAL_ESTRIN_HPP)
#define BALSA_POLYNOMIAL_ESTRIN_HPP
#include <vector>
#include <span>
#include <array>
namespace balsa::polynomial {

namespace detail {
    template<typename Container, typename T>
    typename Container::value_type estrin(Container &c, T v) {
        size_t size = c.size();
        T exp = v;
        while (size > 1) {
            for (std::size_t j = 0; j < size / 2; ++j) {
                c[j] = c[2 * j] + exp * c[2 * j + 1];
            }
            if (size % 2 != 0) {
                c[size / 2 - 1] = c[size - 1];
            }
            size = size / 2;
        }
    }
}// namespace detail


// takes in polynomial coefficients assuming that the lowest indices ahve the degree
template<typename T, std::size_t Extent>
T estrin(const std::span<T, Extent> &coefficients, T v) {
    using Container = std::conditional_t<Extent == std::dynamic_extent, std::vector<T>, std::array<T, Extent>>;
    Container c(coefficients.begin(), coefficients.end());
    return detail::extrin(c, v);
}

}// namespace balsa::polynomial
#endif
