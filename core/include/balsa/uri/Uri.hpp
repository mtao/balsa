#pragma once
#include "UriError.hpp"
#include "detail/UriComponents.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace balsa::uri {

// URI references (RFC 3986 §4.1): absolute URIs ("https://a/b", "data:,x")
// or relative references ("../a.bin"), kept as written. UriView and Uri
// share their whole API (see detail::UriComponents) and differ only in
// ownership, like std::string_view and std::string. Normalization (via ada)
// happens only in resolve(), normalized() and Uri::from_path().

// Non-owning: the viewed text must outlive it.
class UriView : public detail::UriComponents<UriView> {
  public:
    auto text() const -> std::string_view { return m_text; }

  private:
    friend class detail::UriComponents<UriView>;
    friend class Uri;

    explicit UriView(std::string_view text)
      : UriComponents(text), m_text(text) {}
    UriView(std::string_view text, const Offsets &offsets)
      : UriComponents(offsets), m_text(text) {}

    std::string_view m_text;
};

// Owning.
class Uri : public detail::UriComponents<Uri> {
  public:
    // A copy of `view`'s text.
    explicit Uri(UriView view)
      : UriComponents(view.m_offsets), m_text(view.text()) {}

    // Normalized file:// URI for `path`, made absolute first.
    static auto from_path(const std::filesystem::path &path)
        -> std::expected<Uri, UriError>;

    auto text() const -> std::string_view { return m_text; }

    // Borrows text(), so not on temporaries.
    auto view() const & -> UriView { return UriView(m_text, m_offsets); }
    auto view() const && -> UriView = delete;

  private:
    friend class detail::UriComponents<Uri>;
    friend auto detail::uri_resolve(std::string_view, const Uri &)
        -> std::expected<Uri, UriError>;
    friend auto detail::uri_normalized(std::string_view)
        -> std::expected<Uri, UriError>;

    explicit Uri(std::string_view text) : Uri(std::string(text)) {}
    explicit Uri(std::string text)
      : UriComponents(text), m_text(std::move(text)) {}

    std::string m_text;
};

namespace detail {
    template <typename Derived>
    auto UriComponents<Derived>::resolve(const Uri &base) const
        -> std::expected<Uri, UriError> {
        return uri_resolve(whole(), base);
    }

    template <typename Derived>
    auto UriComponents<Derived>::normalized() const
        -> std::expected<Uri, UriError> {
        return uri_normalized(whole());
    }
} // namespace detail

// Decodes %XX escapes; other characters (including invalid escapes) are kept
// as-is.
auto percent_decode(std::string_view text) -> std::string;

} // namespace balsa::uri
