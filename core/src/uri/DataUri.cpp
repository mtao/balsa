#include "balsa/uri/DataUri.hpp"

#include "balsa/encoding/base64.hpp"
#include "balsa/uri/Uri.hpp"
#include "balsa/string/capitalization.hpp"

#include <format>

namespace balsa::uri {

namespace {
    auto trim_whitespace(std::string_view text) -> std::string_view {
        constexpr std::string_view whitespace = " \t\n\r\f";
        const auto first = text.find_first_not_of(whitespace);
        if (first == text.npos) { return {}; }
        const auto last = text.find_last_not_of(whitespace);
        return text.substr(first, last - first + 1);
    }
} // namespace

auto DataUri::parse(UriView uri)
    -> std::expected<DataUri, UriError> {
    const auto is_data = [](std::string_view scheme) {
        return string::ascii::IgnoreCaseEqual{}(scheme, "data");
    };
    if (!uri.scheme().transform(is_data).value_or(false)) {
        return std::unexpected(
            UriError{UriError::Code::InvalidDataUri,
                     std::format("'{}' is not a data: URI", uri.text())});
    }
    const auto body = uri.body();

    const auto comma = body.find(',');
    if (comma == body.npos) {
        return std::unexpected(
            UriError{UriError::Code::InvalidDataUri,
                     std::format("data: URI '{}' has no ','", uri.text())});
    }

    DataUri data;
    data.media_type = trim_whitespace(body.substr(0, comma));
    data.payload = body.substr(comma + 1);
    // A trailing ";base64" parameter (case-insensitive) selects base64.
    if (const auto semicolon = data.media_type.rfind(';');
        semicolon != std::string_view::npos
        && string::ascii::IgnoreCaseEqual{}(
            trim_whitespace(data.media_type.substr(semicolon + 1)), "base64")) {
        data.is_base64 = true;
        data.media_type = trim_whitespace(data.media_type.substr(0, semicolon));
    }
    return data;
}

auto DataUri::decode() const
    -> std::expected<std::vector<std::byte>, UriError> {
    if (!is_base64) {
        const auto text = percent_decode(payload);
        const auto *begin = reinterpret_cast<const std::byte *>(text.data());
        return std::vector<std::byte>(begin, begin + text.size());
    }
    // The payload is percent-encoded too (e.g. "%2B" for '+'), so decode
    // that first; only do the copy when there is something to decode.
    const bool escaped = payload.contains('%');
    const auto decoded = escaped ? percent_decode(payload) : std::string{};
    const auto text = escaped ? std::string_view(decoded) : payload;
    return encoding::base64_decode(text).transform_error(
        [](encoding::Base64Error error) {
            return UriError{UriError::Code::InvalidBase64,
                            "data: URI: " + std::move(error.message)};
        });
}

} // namespace balsa::uri
