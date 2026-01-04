#if !defined(BALSA_FILESYSTEM_PREPEND_TO_FILENAME_HPP)
#define BALSA_FILESYSTEM_PREPEND_TO_FILENAME_HPP


#include <filesystem>

namespace balsa::filesystem {
std::filesystem::path prepend_to_filename(const std::filesystem::path &orig, const std::string &prefix);
}
#endif
