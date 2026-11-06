#pragma once
#include <concepts>
#include <cstddef>
#include <flat_map>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

namespace balsa::catalog {

// Something a Catalog can hold: it has an `id`, its stable name.
template <typename T>
concept Identified = requires(const T &t) {
    { t.id } -> std::convertible_to<std::string_view>;
};

template <Identified T>
class Catalog;

// An entry of a Catalog: stores a pointer to identifiable data the catalog
// owns, compared by identity.  Only catalogs hand these out.
template <Identified T>
class Entry {
  public:
    [[nodiscard]] auto id() const -> std::string_view { return m_value->id; }
    [[nodiscard]] auto value() const -> const T & { return *m_value; }
    auto operator==(const Entry &) const -> bool = default;

  private:
    friend class Catalog<T>;
    explicit Entry(const T *value) : m_value(value) {}
    const T *m_value;
};

// Values keyed by their id, kept at stable addresses so entries stay valid
// (also when the catalog is moved).  Sorted by id for lookup; callers that
// list entries choose their own order.
template <Identified T>
class Catalog {
  public:
    // `kind` names the values in log messages.
    explicit Catalog(std::string_view kind) : m_kind(kind) {}

    // Adds `value`, replacing (with a debug message) any with the same id;
    // entries for it then see the new value.
    auto add(T value) -> Entry<T> {
        std::string id(value.id);
        if (const auto it = m_items.find(id); it != m_items.end()) {
            spdlog::debug("replacing {} '{}'", m_kind, id);
            *it->second = std::move(value);
            return Entry<T>(it->second.get());
        }
        const auto [it, inserted] = m_items.emplace(
            std::move(id), std::make_unique<T>(std::move(value)));
        return Entry<T>(it->second.get());
    }
    // nullopt if `id` is not in the catalog.
    [[nodiscard]] auto find(std::string_view id) const
        -> std::optional<Entry<T>> {
        const auto it = m_items.find(id);
        if (it == m_items.end()) return std::nullopt;
        return Entry<T>(it->second.get());
    }
    // In no particular order.
    [[nodiscard]] auto all() const -> std::vector<Entry<T>> {
        std::vector<Entry<T>> result;
        result.reserve(m_items.size());
        for (const auto &[id, value] : m_items) result.push_back(Entry<T>(value.get()));
        return result;
    }

  private:
    std::string m_kind;
    std::flat_map<std::string, std::unique_ptr<T>, std::less<>> m_items;
};

} // namespace balsa::catalog

template <balsa::catalog::Identified T>
struct std::hash<balsa::catalog::Entry<T>> {
    auto operator()(const balsa::catalog::Entry<T> &e) const -> std::size_t {
        return std::hash<const void *>{}(&e.value());
    }
};
