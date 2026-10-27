#include "balsa/uri/detail/UriComponents.hpp"

#include <algorithm>
#include <format>

namespace balsa::uri::detail {

auto split_uri_reference(std::string_view text) -> UriOffsets {
    // RFC 3986 Appendix B:
    //   ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
    const auto at = [](UriSection s) { return std::to_underlying(s); };
    UriOffsets offsets;
    offsets.fill(omitted_offset);
    offsets[at(UriSection::End)] = static_cast<std::uint32_t>(text.size());

    std::size_t pos = 0;
    if (const auto colon = text.find_first_of(":/?#");
        colon != text.npos && colon > 0 && text[colon] == ':') {
        offsets[at(UriSection::Scheme)] = 0;
        pos = colon + 1;
    }
    if (text.substr(pos).starts_with("//")) {
        offsets[at(UriSection::Authority)] = static_cast<std::uint32_t>(pos);
        pos = std::min(text.find_first_of("/?#", pos + 2), text.size());
    }
    offsets[at(UriSection::Path)] = static_cast<std::uint32_t>(pos);
    if (const auto question = text.find_first_of("?#", pos);
        question != text.npos && text[question] == '?') {
        offsets[at(UriSection::Query)] = static_cast<std::uint32_t>(question);
    }
    if (const auto hash = text.find('#', pos); hash != text.npos) {
        offsets[at(UriSection::Fragment)] = static_cast<std::uint32_t>(hash);
    }
    return offsets;
}

namespace {
    // Raw end of `s` (including its delimiters): the start of the next
    // present section. End is never omitted, so one is always found.
    auto section_end(const UriOffsets &offsets, UriSection s) -> std::uint32_t {
        return *std::ranges::find_if(
            offsets.begin() + std::to_underlying(s) + 1, offsets.end(),
            [](std::uint32_t start) { return start != omitted_offset; });
    }
} // namespace

auto uri_section(std::string_view text, const UriOffsets &offsets,
                 UriSection s) -> std::optional<std::string_view> {
    // Delimiter characters to strip from each section: {prefix, suffix}.
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>,
                         std::to_underlying(UriSection::End)>
        delimiters{{{0, 1}, {2, 0}, {0, 0}, {1, 0}, {1, 0}}};

    const auto i = std::to_underlying(s);
    const auto start = offsets[i];
    if (start == omitted_offset) { return std::nullopt; }
    const auto end = section_end(offsets, s);
    const auto [prefix, suffix] = delimiters[i];
    const auto local_start = start + prefix;
    return text.substr(local_start, end - local_start - suffix);
}

auto uri_body(std::string_view text, const UriOffsets &offsets)
    -> std::string_view {
    const auto start = offsets[std::to_underlying(UriSection::Path)];
    return text.substr(start, section_end(offsets, UriSection::Query) - start);
}

namespace {
    auto is_ascii_alpha(char c) -> bool {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    auto is_scheme(std::string_view text) -> bool {
        return !text.empty() && is_ascii_alpha(text.front())
               && std::ranges::all_of(text, [](char c) {
                      return is_ascii_alpha(c) || (c >= '0' && c <= '9')
                             || c == '+' || c == '-' || c == '.';
                  });
    }
} // namespace

auto validate_uri_reference(std::string_view text, const UriOffsets &offsets)
    -> std::expected<void, UriError> {
    // A first segment containing ':' must be a scheme; a relative path like
    // "a:b" has to be written "./a:b".
    if (const auto scheme = uri_section(text, offsets, UriSection::Scheme);
        scheme && !is_scheme(*scheme)) {
        return std::unexpected(UriError{
            UriError::Code::InvalidUri,
            std::format("invalid scheme in URI reference '{}'", text)});
    }
    return {};
}

auto uri_is_path_relative(std::string_view text, const UriOffsets &offsets)
    -> bool {
    const auto path = *uri_section(text, offsets, UriSection::Path);
    return !uri_section(text, offsets, UriSection::Scheme)
           && !uri_section(text, offsets, UriSection::Authority)
           && !path.starts_with('/') && !path.starts_with('\\');
}

} // namespace balsa::uri::detail
