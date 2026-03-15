#if !defined(BALSA_TYPES_GET_TYPE_NAME_HPP)
#define BALSA_TYPES_GET_TYPE_NAME_HPP

#include <string>
#include <typeinfo>
#include <type_traits>

namespace balsa::types {

namespace detail {
    std::string demangle_type_name(const char *const name);
}
template<typename T>
std::string get_type_name() {
    const char *const name = typeid(T).name();
    std::string result = detail::demangle_type_name(name);
    if constexpr (std::is_lvalue_reference_v<T>) {
        result += "&";
    } else if constexpr (std::is_rvalue_reference_v<T>) {
        result += "&&";
    }
    return result;
}
}// namespace balsa::types

#endif
