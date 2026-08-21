
#include "balsa/filesystem/prepend_to_filename.hpp"

namespace balsa::filesystem {
std::filesystem::path prepend_to_filename(const std::filesystem::path &orig, const std::string &prefix) {
    auto parent = orig.parent_path();
    auto filename = orig.filename();
    // std::filesystem::path is not implicitly convertible to std::string on MSVC
    return parent / (prefix + filename.string());
}
}// namespace balsa::filesystem
