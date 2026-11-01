#include "balsa/uri/Resolver.hpp"

#include "balsa/uri/DataUri.hpp"
#include "balsa/string/capitalization.hpp"

#include <format>
#include <fstream>

namespace balsa::uri {

namespace {
    auto fetch_data(UriView uri)
        -> std::expected<Resolver::Bytes, UriError> {
        return DataUri::parse(uri).and_then(&DataUri::decode);
    }

    auto read_file(const std::filesystem::path &path)
        -> std::expected<Resolver::Bytes, UriError> {
        const auto io_error = [&](std::string_view what) {
            return std::unexpected(UriError{
                UriError::Code::Io,
                std::format("cannot {} '{}'", what, path.string())});
        };
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs) { return io_error("open"); }
        Resolver::Bytes bytes(static_cast<std::size_t>(ifs.tellg()));
        ifs.seekg(0);
        if (!ifs.read(reinterpret_cast<char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()))) {
            return io_error("read");
        }
        return bytes;
    }

    auto fetch_file(UriView uri) -> std::expected<Resolver::Bytes, UriError> {
        return uri.to_path().and_then(read_file);
    }
} // namespace

auto Resolver::defaults() -> Resolver {
    return Resolver{}.with("data", fetch_data).with("file", fetch_file);
}

auto Resolver::with(std::string scheme, Fetch fetch) const & -> Resolver {
    return Resolver(*this).with(std::move(scheme), std::move(fetch));
}

auto Resolver::with(std::string scheme, Fetch fetch) && -> Resolver {
    m_fetchers.insert_or_assign(std::move(scheme), std::move(fetch));
    return std::move(*this);
}

auto Resolver::handles(std::string_view scheme) const -> bool {
    return m_fetchers.contains(scheme);
}

auto Resolver::fetch(UriView uri) const
    -> std::expected<Bytes, UriError> {
    if (uri.is_relative()) {
        return std::unexpected(UriError{
            UriError::Code::InvalidUri,
            std::format("cannot fetch relative URI '{}'", uri.text())});
    }
    const auto scheme = string::ascii::tolower(*uri.scheme());
    const auto it = m_fetchers.find(scheme);
    if (it == m_fetchers.end()) {
        return std::unexpected(UriError{
            UriError::Code::UnsupportedScheme,
            std::format("no handler for the '{}' scheme of '{}'", scheme,
                        uri.text())});
    }
    return it->second(uri);
}

} // namespace balsa::uri
