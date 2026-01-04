#if !defined(BALSA_TYPES_HAS_TUPLE_SIZE_HPP)
#define BALSA_TYPES_HAS_TUPLE_SIZE_HPP

namespace balsa::types {

template<typename T>
concept has_tuple_size = requires(T) {
    std::tuple_size<T>();
};
//template<typename T, class = void>
//struct has_tuple_size : public std::false_type {};
//
//template<class T>
//struct has_tuple_size<
//  T,
//  std::enable_if_t<std::tuple_size<T>() == std::tuple_size<T>()>>
//  : public std::true_type {};
//}
}// namespace balsa::types

#endif
