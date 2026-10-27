#pragma once
#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace balsa::uri {

struct UriError {
    enum class Code {
        // The text is not a valid URI (or not resolvable against the base).
        InvalidUri,
        // to_path() on a URI whose scheme is not file: or that has a host.
        NotAFileUri,
        // The decoded path is not usable (e.g. contains a NUL byte).
        InvalidPath,
    };
    Code code;
    std::string message;
};

// A parsed, normalized absolute URI (WHATWG URL semantics, via ada).
// Component accessors return views into href().
class Uri {
  public:
    static auto parse(std::string_view text) -> std::expected<Uri, UriError>;
    // Resolves `reference` against `base` (RFC 3986 §5), e.g. a relative
    // glTF URI against the asset's URI.
    static auto parse(std::string_view reference, const Uri &base)
        -> std::expected<Uri, UriError>;
    // file:// URI for `path`, made absolute first.
    static auto from_path(const std::filesystem::path &path)
        -> std::expected<Uri, UriError>;

    // Without the trailing ':', e.g. "file", "data", "https".
    auto scheme() const -> std::string_view;
    // Still percent-encoded.
    auto path() const -> std::string_view;
    // Without the leading '?' / '#'.
    auto query() const -> std::optional<std::string_view>;
    auto fragment() const -> std::optional<std::string_view>;
    auto href() const -> std::string_view { return m_href; }

    // Percent-decoded local path of a file: URI.
    auto to_path() const -> std::expected<std::filesystem::path, UriError>;

  private:
    friend struct UriAccess; // fills the members from ada in Uri.cpp

    // Sections in href order. Each starts at its offset (including its
    // delimiter) and ends where the next present section starts; End is a
    // sentinel at m_href.size().
    enum class Section : std::uint8_t {
        Scheme, // "file:"
        Authority, // "//host", possibly empty
        Path,
        Query, // "?..."
        Fragment, // "#..."
        End,
    };
    auto section(Section s) const -> std::optional<std::string_view>;

    std::string m_href;
    // Start of each section in m_href (UINT32_MAX if absent), then End.
    std::array<std::uint32_t, static_cast<std::size_t>(Section::End) + 1>
        m_offsets{};
};

} // namespace balsa::uri
