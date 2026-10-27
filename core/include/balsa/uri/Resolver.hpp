#pragma once
#include "UriError.hpp"
#include "Uri.hpp"

#include <cstddef>
#include <expected>
#include <flat_map>
#include <functional>
#include <string>
#include <vector>

namespace balsa::uri {

// Fetches the bytes an absolute URI refers to, dispatching on its scheme
// (case-insensitively). Pass a Uri via Uri::view().
//
// A plain value: copying it copies its handlers. Handlers must be cheap to
// copy and safe to call concurrently; one that owns an expensive or stateful
// resource (a connection pool, an open archive) should capture it through a
// shared_ptr so copies share it.
class Resolver {
  public:
    using Bytes = std::vector<std::byte>;
    using Fetch = std::copyable_function<
        std::expected<Bytes, UriError>(UriView) const>;

    // Handles data: (decoded) and file: (read from disk).
    static auto defaults() -> Resolver;

    // A copy with `fetch` handling `scheme` (lowercase; replacing any
    // existing handler).
    auto with(std::string scheme, Fetch fetch) const & -> Resolver;
    auto with(std::string scheme, Fetch fetch) && -> Resolver;

    auto handles(std::string_view scheme) const -> bool;
    // `uri` must be absolute.
    auto fetch(UriView uri) const
        -> std::expected<Bytes, UriError>;

  private:
    std::flat_map<std::string, Fetch, std::less<>> m_fetchers;
};

} // namespace balsa::uri
