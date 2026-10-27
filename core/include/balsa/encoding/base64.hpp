#pragma once
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace balsa::encoding {

struct Base64Error {
    // Offset in the input of the offending character.
    std::size_t position;
    std::string message;
};

// Standard alphabet (RFC 4648 §4), strict: the final chunk must be padded
// and its unused bits zero. ASCII whitespace is ignored.
auto base64_decode(std::string_view text)
    -> std::expected<std::vector<std::byte>, Base64Error>;

// Standard alphabet, padded.
auto base64_encode(std::span<const std::byte> bytes) -> std::string;

} // namespace balsa::encoding
