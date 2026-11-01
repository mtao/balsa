#pragma once
#include "UriError.hpp"
#include "Uri.hpp"
#include "balsa/string/capitalization.hpp"

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
    using bytes_type = std::vector<std::byte>;
    using result_type = std::expected<bytes_type, UriError>;
    using fetch_function_type =
        std::copyable_function<result_type(UriView) const>;

    // Handles data: (decoded) and file: (read from disk).
    static auto defaults() -> Resolver;

    // Makes `fetch` handle `scheme`, replacing any existing handler.
    auto add(std::string_view scheme, fetch_function_type fetch) & -> Resolver &;
    auto add(std::string_view scheme, fetch_function_type fetch) && -> Resolver &&;

    auto handles(std::string_view scheme) const -> bool;
    // `uri` must be absolute.
    auto fetch(UriView uri) const -> result_type;

  private:
    std::flat_map<std::string, fetch_function_type, string::ascii::IgnoreCaseLess>
        m_fetchers;
};

} // namespace balsa::uri
