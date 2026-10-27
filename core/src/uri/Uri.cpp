#include "balsa/uri/Uri.hpp"

#include <ada.h>

#include <algorithm>
#include <format>
#include <utility>

namespace balsa::uri {

struct UriAccess {
    static auto from_ada(const ada::url_aggregator &url) -> Uri {
        const auto &c = url.get_components();
        Uri uri;
        uri.m_href = std::string(url.get_href());
        uri.m_offsets = {0,
                         c.protocol_end,
                         c.pathname_start,
                         c.search_start,
                         c.hash_start,
                         static_cast<std::uint32_t>(uri.m_href.size())};
        return uri;
    }
};

namespace {
    // Marks an absent section in Uri::m_offsets; same as ada's.
    constexpr std::uint32_t omitted = ada::url_components::omitted;

    auto invalid(std::string_view text) -> std::unexpected<UriError> {
        return std::unexpected(UriError{UriError::Code::InvalidUri,
                                        std::format("invalid URI '{}'", text)});
    }

} // namespace

auto Uri::section_end(Section s) const -> std::uint32_t {
    // End is never omitted, so this always finds one.
    return *std::ranges::find_if(
        m_offsets.begin() + std::to_underlying(s) + 1, m_offsets.end(),
        [](std::uint32_t start) { return start != omitted; });
}

auto Uri::parse(std::string_view text) -> std::expected<Uri, UriError> {
    const auto url = ada::parse<ada::url_aggregator>(text);
    if (!url) { return invalid(text); }
    return UriAccess::from_ada(*url);
}

auto Uri::parse(std::string_view reference, const Uri &base)
    -> std::expected<Uri, UriError> {
    const auto base_url = ada::parse<ada::url_aggregator>(base.href());
    if (!base_url) { return invalid(base.href()); }
    const auto url = ada::parse<ada::url_aggregator>(reference, &*base_url);
    if (!url) { return invalid(reference); }
    return UriAccess::from_ada(*url);
}

auto Uri::from_path(const std::filesystem::path &path)
    -> std::expected<Uri, UriError> {
    const auto absolute = std::filesystem::absolute(path).generic_string();
    const auto href = ada::href_from_file(absolute);
    if (href.empty()) { return invalid(absolute); }
    return parse(href);
}

auto Uri::section(Section s) const -> std::optional<std::string_view> {
    // Delimiter characters to strip from each section: {prefix, suffix}.
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>,
                         static_cast<std::size_t>(Section::End)>
        delimiters{{{0, 1}, {0, 0}, {0, 0}, {1, 0}, {1, 0}}};

    const auto i = static_cast<std::size_t>(s);
    const auto start = m_offsets[i];
    if (start == omitted) { return std::nullopt; }
    const auto end = section_end(s);
    const auto [prefix, suffix] = delimiters[i];

    const auto local_start = start + prefix;
    return std::string_view(m_href).substr(local_start,
                                           end - local_start - suffix);
}

auto Uri::scheme() const -> std::string_view {
    return *section(Section::Scheme);
}

auto Uri::body() const -> std::string_view {
    const auto start = m_offsets[static_cast<std::size_t>(Section::Path)];
    const auto end = section_end(Section::Query);
    return std::string_view(m_href).substr(start, end - start);
}

auto Uri::path() const -> std::string_view { return *section(Section::Path); }

auto Uri::query() const -> std::optional<std::string_view> {
    return section(Section::Query);
}

auto Uri::fragment() const -> std::optional<std::string_view> {
    return section(Section::Fragment);
}

auto Uri::to_path() const -> std::expected<std::filesystem::path, UriError> {
    // "//" alone means no host (ada normalizes file://localhost/ to that);
    // file URLs carry no userinfo or port.
    if (scheme() != "file" || section(Section::Authority)->size() > 2) {
        return std::unexpected(
            UriError{UriError::Code::NotAFileUri,
                     std::format("'{}' is not a local file URI", m_href)});
    }
    auto decoded = percent_decode(path());
    if (decoded.find('\0') != std::string::npos) {
        return std::unexpected(UriError{
            UriError::Code::InvalidPath,
            std::format("'{}' decodes to a path with a NUL byte", m_href)});
    }
    return std::filesystem::path(std::move(decoded));
}

auto percent_decode(std::string_view text) -> std::string {
    return ada::unicode::percent_decode(text, text.find('%'));
}

} // namespace balsa::uri
