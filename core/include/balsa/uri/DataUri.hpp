#pragma once
#include "Uri.hpp"

#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

namespace balsa::uri {

// A data: URI (RFC 2397): "data:[<media type>][;base64],<payload>".
// Views point into the Uri's href(), which must outlive this object.
struct DataUri {
    // As written, e.g. "application/octet-stream" or
    // "text/plain;charset=utf-8"; empty means text/plain;charset=US-ASCII.
    std::string_view media_type;
    bool is_base64 = false;
    // Still encoded (base64 or percent-encoded); excludes any #fragment.
    std::string_view payload;

    static auto parse(const Uri &uri) -> std::expected<DataUri, UriError>;

    auto decode() const -> std::expected<std::vector<std::byte>, UriError>;
};

} // namespace balsa::uri
