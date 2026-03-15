#include "balsa/types/get_type_name.hpp"

#include <cstdlib>
#include <string>
#if !defined(_MSC_VER)
#include <cxxabi.h>
#endif

namespace balsa::types::detail {

std::string demangle_type_name(const char *const name) {
    int status = 0;
    // TODO: Actually demangle in msc
#if defined(_MSC_VER)
    const char *const readable_name = name;
#else
    using abi::__cxa_demangle;
    char *const readable_name = __cxa_demangle(name, 0, 0, &status);
#endif
    std::string name_str(status == 0 ? readable_name : name);
#if !defined(_MSC_VER)
    free(readable_name);
#endif
    return name_str;
}

}// namespace balsa::types::detail
