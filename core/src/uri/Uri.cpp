#include "balsa/uri/Uri.hpp"

#include <ada.h>

#include <format>

namespace balsa::uri {

namespace {
    auto invalid(std::string message) -> std::unexpected<UriError> {
        return std::unexpected(
            UriError{UriError::Code::InvalidUri, std::move(message)});
    }

    auto parse_ada(std::string_view text, const ada::url_aggregator *base)
        -> std::expected<ada::url_aggregator, UriError> {
        auto url = ada::parse<ada::url_aggregator>(text, base);
        if (!url) { return invalid(std::format("invalid URI '{}'", text)); }
        return std::move(*url);
    }
} // namespace

namespace detail {

    auto uri_resolve(std::string_view text, const Uri &base)
        -> std::expected<Uri, UriError> {
        if (base.is_relative()) {
            return invalid(std::format("cannot resolve '{}' against "
                                       "relative base '{}'",
                                       text, base.text()));
        }
        const auto base_url = parse_ada(base.text(), nullptr);
        if (!base_url) { return std::unexpected(base_url.error()); }
        return parse_ada(text, &*base_url).transform([](const auto &url) {
            return Uri(std::string(url.get_href()));
        });
    }

    auto uri_normalized(std::string_view text) -> std::expected<Uri, UriError> {
        return parse_ada(text, nullptr).transform([](const auto &url) {
            return Uri(std::string(url.get_href()));
        });
    }

    auto uri_to_path(std::string_view text)
        -> std::expected<std::filesystem::path, UriError> {
        // Normalize first ("a/../b", "FILE:", file://localhost/ -> file:///).
        const auto uri = uri_normalized(text);
        const auto not_file = [&] {
            return std::unexpected(
                UriError{UriError::Code::NotAFileUri,
                         std::format("'{}' is not a local file URI", text)});
        };
        if (!uri) { return not_file(); }
        // An empty authority means no host; file URLs carry no userinfo or
        // port.
        if (uri->scheme() != "file" || !uri->authority()
            || !uri->authority()->empty()) {
            return not_file();
        }
        auto decoded = percent_decode(uri->path());
        if (decoded.find('\0') != std::string::npos) {
            return std::unexpected(UriError{
                UriError::Code::InvalidPath,
                std::format("'{}' decodes to a path with a NUL byte", text)});
        }
        return std::filesystem::path(std::move(decoded));
    }

} // namespace detail

auto Uri::from_path(const std::filesystem::path &path)
    -> std::expected<Uri, UriError> {
    const auto absolute = std::filesystem::absolute(path).generic_string();
    const auto href = ada::href_from_file(absolute);
    if (href.empty()) {
        return invalid(std::format("cannot make a URI from '{}'", absolute));
    }
    return Uri(href);
}

auto percent_decode(std::string_view text) -> std::string {
    return ada::unicode::percent_decode(text, text.find('%'));
}

} // namespace balsa::uri
