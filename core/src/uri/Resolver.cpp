#include "balsa/uri/Resolver.hpp"

#include "balsa/uri/DataUri.hpp"

#include <format>
#include <fstream>

namespace balsa::uri {

namespace {
    auto fetch_data(UriView uri)
        -> Resolver::result_type {
        return DataUri::parse(uri).and_then(&DataUri::decode);
    }

    auto read_file(const std::filesystem::path &path)
        -> Resolver::result_type {
        const auto io_error = [&](std::string_view what) {
            return std::unexpected(UriError{
                UriError::Code::Io,
                std::format("cannot {} '{}'", what, path.string())});
        };
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs) { return io_error("open"); }
        Resolver::bytes_type bytes(static_cast<std::size_t>(ifs.tellg()));
        ifs.seekg(0);
        if (!ifs.read(reinterpret_cast<char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()))) {
            return io_error("read");
        }
        return bytes;
    }

    auto fetch_file(UriView uri) -> Resolver::result_type {
        return uri.to_path().and_then(read_file);
    }
} // namespace

auto Resolver::defaults() -> Resolver {
    return Resolver{}.add("data", fetch_data).add("file", fetch_file);
}

auto Resolver::add(std::string_view scheme, fetch_function_type fetch) &
    -> Resolver & {
    m_fetchers.insert_or_assign(std::string(scheme), std::move(fetch));
    return *this;
}

auto Resolver::add(std::string_view scheme, fetch_function_type fetch) &&
    -> Resolver && {
    return std::move(add(scheme, std::move(fetch)));
}

auto Resolver::handles(std::string_view scheme) const -> bool {
    return m_fetchers.contains(scheme);
}

auto Resolver::fetch(UriView uri) const -> result_type {
    if (uri.is_relative()) {
        return std::unexpected(UriError{
            UriError::Code::InvalidUri,
            std::format("cannot fetch relative URI '{}'", uri.text())});
    }
    const auto scheme = *uri.scheme();
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
