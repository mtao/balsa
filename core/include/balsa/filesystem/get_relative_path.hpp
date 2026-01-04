#if !defined(BALSA_FILESYSTEM_GET_RELATIVE_PATH_HPP)
#define BALSA_FILESYSTEM_GET_RELATIVE_PATH_HPP
#include <filesystem>

namespace balsa::filesystem {


// given a file "/a/b/c/base_file" and a path "../d/e/file2" this function returns
// /a/b/c/../d/e/file2 if such a file exists
std::filesystem::path get_relative_path(const std::filesystem::path &base_file, const std::string &new_path);

}// namespace balsa::filesystem
#endif
