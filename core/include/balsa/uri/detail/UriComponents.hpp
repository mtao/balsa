#pragma once
#include "../UriError.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

namespace balsa::uri {
class Uri;
} // namespace balsa::uri

namespace balsa::uri::detail {

// Sections of a URI reference in text order. Each starts at its offset
// (including its delimiter) and ends where the next present section starts;
// End is a sentinel at the text's size.
enum class UriSection : std::uint8_t {
    Scheme,     // "file:"
    Authority,  // "//host"
    Path,       // always present, possibly empty
    Query,      // "?..."
    Fragment,   // "#..."
    End,
};
// Start of each section (`omitted_offset` if absent), then End.
using UriOffsets =
    std::array<std::uint32_t, std::to_underlying(UriSection::End) + 1>;
inline constexpr std::uint32_t omitted_offset = UINT32_MAX;

// Non-template implementations (UriComponents.cpp / Uri.cpp), operating on
// the text and its offsets.

// Splits `text` into sections per RFC 3986 Appendix B. Always succeeds:
// the scheme section is not validated (e.g. "1x:" gives scheme "1x").
auto split_uri_reference(std::string_view text) -> UriOffsets;
// Fails if the scheme section is not a valid RFC 3986 scheme.
auto validate_uri_reference(std::string_view text, const UriOffsets &offsets)
    -> std::expected<void, UriError>;
// Section `s` without its delimiters (':', "//", '?', '#'); nullopt if absent.
auto uri_section(std::string_view text, const UriOffsets &offsets,
                 UriSection s) -> std::optional<std::string_view>;
// Path and query, without the fragment.
auto uri_body(std::string_view text, const UriOffsets &offsets)
    -> std::string_view;
auto uri_is_path_relative(std::string_view text, const UriOffsets &offsets)
    -> bool;
auto uri_resolve(std::string_view text, const Uri &base)
    -> std::expected<Uri, UriError>;
auto uri_normalized(std::string_view text) -> std::expected<Uri, UriError>;
auto uri_to_path(std::string_view text)
    -> std::expected<std::filesystem::path, UriError>;

// Everything UriView and Uri share: they differ only in whether they own
// their text. Derived provides text() and a private constructor from
// std::string_view (befriending this class).
template <typename Derived>
class UriComponents {
  public:
    // Any URI reference (RFC 3986 §4.1): absolute or relative, as written.
    static auto parse(std::string_view text)
        -> std::expected<Derived, UriError>;
    // An absolute URI (with a scheme), as written.
    static auto parse_absolute(std::string_view text)
        -> std::expected<Derived, UriError>;

    // Without the trailing ':', e.g. "file", "data", "HTTPS"; as written.
    auto scheme() const -> std::optional<std::string_view> {
        return section(UriSection::Scheme);
    }
    // Without the leading "//"; empty for e.g. "file:///x".
    auto authority() const -> std::optional<std::string_view> {
        return section(UriSection::Authority);
    }
    // Still percent-encoded.
    auto path() const -> std::string_view { return *section(UriSection::Path); }
    // Path and query (without the fragment), still percent-encoded, e.g. the
    // "<type>;base64,<data>" of a data: URI. Always present, possibly empty.
    auto body() const -> std::string_view { return uri_body(whole(), m_offsets); }
    // Without the leading '?' / '#'.
    auto query() const -> std::optional<std::string_view> {
        return section(UriSection::Query);
    }
    auto fragment() const -> std::optional<std::string_view> {
        return section(UriSection::Fragment);
    }

    // Has a scheme.
    auto is_absolute() const -> bool { return scheme().has_value(); }
    // No scheme: must be resolved against an absolute base before use.
    auto is_relative() const -> bool { return !is_absolute(); }
    // A relative reference with no authority and a path that does not start
    // with '/' (or '\', which ada treats as '/'), e.g. "a.bin" or
    // "../tex/a.png": the kind that stays relative to its base's directory.
    auto is_path_relative() const -> bool {
        return uri_is_path_relative(whole(), m_offsets);
    }

    // RFC 3986 §5 resolution against an absolute `base` (via ada). The
    // result is normalized; an absolute reference resolves to itself.
    // (Defined in Uri.hpp, where Uri is complete.)
    auto resolve(const Uri &base) const -> std::expected<Uri, UriError>;
    // Normalized form (via ada): lowercase scheme, "." / ".." segments
    // removed, file://localhost/ -> file:///, etc. Must be absolute.
    auto normalized() const -> std::expected<Uri, UriError>;
    // Percent-decoded local path of an absolute file: URI without a host.
    auto to_path() const -> std::expected<std::filesystem::path, UriError> {
        return uri_to_path(whole());
    }

  protected:
    using Offsets = UriOffsets;

    // Offsets of `text`; only the offsets are kept, so `text` may be a
    // temporary the derived class then moves into its own storage.
    explicit UriComponents(std::string_view text)
      : m_offsets(split_uri_reference(text)) {}
    // Offsets already known (e.g. copied from another object).
    explicit UriComponents(const Offsets &offsets) : m_offsets(offsets) {}

    Offsets m_offsets;

  private:
    auto whole() const -> std::string_view {
        return static_cast<const Derived &>(*this).text();
    }
    auto section(UriSection s) const -> std::optional<std::string_view> {
        return uri_section(whole(), m_offsets, s);
    }
};

template <typename Derived>
auto UriComponents<Derived>::parse(std::string_view text)
    -> std::expected<Derived, UriError> {
    Derived uri(text);
    if (auto valid = validate_uri_reference(uri.text(), uri.m_offsets);
        !valid) {
        return std::unexpected(std::move(valid.error()));
    }
    return uri;
}

template <typename Derived>
auto UriComponents<Derived>::parse_absolute(std::string_view text)
    -> std::expected<Derived, UriError> {
    auto uri = parse(text);
    if (uri && uri->is_relative()) {
        return std::unexpected(UriError{
            UriError::Code::InvalidUri,
            "expected an absolute URI, got relative reference '"
                + std::string(text) + "'"});
    }
    return uri;
}

} // namespace balsa::uri::detail
