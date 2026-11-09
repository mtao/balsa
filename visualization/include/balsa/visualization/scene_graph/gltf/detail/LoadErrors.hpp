#pragma once
#include "../LoadError.hpp"

#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace balsa::visualization::gltf::detail {

// Collects load problems into one LoadError.
class LoadErrors {
  public:
    void add(LoadError::Code code, std::string message) {
        m_errors.push_back(LoadError{code, std::move(message)});
    }
    void add(LoadError error) { m_errors.push_back(std::move(error)); }
    // Adds `error`, prefixed with `context`.
    void add(std::string_view context, LoadError error) {
        error.message = std::format("{}: {}", context, error.message);
        m_errors.push_back(std::move(error));
    }

    auto empty() const -> bool { return m_errors.empty(); }

    // One LoadError joining every message.
    auto combined() && -> LoadError {
        if (m_errors.size() == 1) { return std::move(m_errors.front()); }
        auto messages =
            m_errors
            | std::views::transform(&LoadError::message)
            | std::views::join_with(std::string_view("; "))
            | std::ranges::to<std::string>();
        return LoadError{
            m_errors.front().code,
            std::format("{} errors: {}", m_errors.size(), messages)};
    }

  private:
    std::vector<LoadError> m_errors;
};

} // namespace balsa::visualization::gltf::detail
