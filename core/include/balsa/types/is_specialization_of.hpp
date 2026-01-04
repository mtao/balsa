#if !defined(BALSA_TYPES_IS_SPECIALIZATION_OF_HPP)
#define BALSA_TYPES_IS_SPECIALIZATION_OF_HPP

#include <type_traits>

namespace balsa::types {

// https://stackoverflow.com/questions/11251376/how-can-i-check-if-a-type-is-an-instantiation-of-a-given-class-template#comment14786989_11251408
template<template<typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template<template<typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

template<template<typename...> class Template, typename T>
constexpr bool is_specialization_of_v =
  is_specialization_of<Template, T>::value;


}// namespace balsa::types


#endif
