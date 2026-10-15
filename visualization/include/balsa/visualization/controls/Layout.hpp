#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "Property.hpp"

namespace balsa::visualization::controls {

class Layout {
  public:
    enum class Direction { Column, Row };

    explicit Layout(Direction d = Direction::Column, std::string heading = {})
      : m_direction(d), m_heading(std::move(heading)) {}

    Layout(Layout &&) = default;
    auto operator=(Layout &&) -> Layout & = default;

    auto add_property(Property p) -> Layout & {
        m_items.emplace_back(std::move(p));
        return *this;
    }

    /// Nested horizontal / vertical group.  The returned reference stays
    /// valid until the parent Layout is destroyed.
    auto add_row(std::string heading = {}) -> Layout & {
        return child(Direction::Row, std::move(heading));
    }
    auto add_column(std::string heading = {}) -> Layout & {
        return child(Direction::Column, std::move(heading));
    }

    auto set_visible_when(Predicate p) -> Layout & {
        m_visible = std::move(p);
        return *this;
    }

    using Item = std::variant<Property, std::unique_ptr<Layout>>;

    auto direction() const -> Direction { return m_direction; }
    auto heading() const -> const std::string & { return m_heading; }
    auto visible_when() const -> const Predicate & { return m_visible; }
    auto items() const -> const std::vector<Item> & { return m_items; }

    template <typename F>
    auto for_each_property(F &&f) const -> void {
        for (const auto &item : m_items) {
            if (const auto *p = std::get_if<Property>(&item)) {
                f(*p);
            } else {
                std::get<std::unique_ptr<Layout>>(item)->for_each_property(f);
            }
        }
    }

  private:
    auto child(Direction d, std::string heading) -> Layout & {
        auto &slot = m_items.emplace_back(
            std::make_unique<Layout>(d, std::move(heading)));
        return *std::get<std::unique_ptr<Layout>>(slot);
    }

    Direction m_direction;
    std::string m_heading;
    Predicate m_visible = always_true;
    std::vector<Item> m_items;
};

} // namespace balsa::visualization::controls
