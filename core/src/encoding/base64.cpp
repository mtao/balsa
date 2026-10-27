#include "balsa/encoding/base64.hpp"

#include <simdutf.h>

#include <format>

namespace balsa::encoding {

auto base64_decode(std::string_view text)
    -> std::expected<std::vector<std::byte>, Base64Error> {
    std::vector<std::byte> bytes(
        simdutf::maximal_binary_length_from_base64(text.data(), text.size()));
    const auto result = simdutf::base64_to_binary(
        text.data(), text.size(), reinterpret_cast<char *>(bytes.data()),
        simdutf::base64_default, simdutf::last_chunk_handling_options::strict);
    if (result.error != simdutf::error_code::SUCCESS) {
        return std::unexpected(Base64Error{
            result.count,
            std::format("invalid base64 at offset {} ({})", result.count,
                        simdutf::error_to_string(result.error))});
    }
    bytes.resize(result.count);
    return bytes;
}

auto base64_encode(std::span<const std::byte> bytes) -> std::string {
    std::string text(simdutf::base64_length_from_binary(bytes.size()), '\0');
    const auto written = simdutf::binary_to_base64(
        reinterpret_cast<const char *>(bytes.data()), bytes.size(), text.data());
    text.resize(written);
    return text;
}

} // namespace balsa::encoding
