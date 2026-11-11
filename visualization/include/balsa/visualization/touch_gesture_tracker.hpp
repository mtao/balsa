#pragma once
#include <algorithm>
#include <flat_map>
#include <optional>
#include <ranges>

#include <zipper/Vector.hpp>

#include "balsa/visualization/input_handler.hpp"

namespace balsa::visualization {

// Incremental motion of a set of touch contacts between two TouchEvents.
//
//   touch_count  number of contacts that stayed down for the whole step
//   translation  movement of their centroid (logical pixels)
//   scale        change in spread (>1 = fingers moving apart); always 1
//                for a single contact
struct TouchGesture {
    int touch_count = 0;
    zipper::Vector<double, 2> translation{ 0.0, 0.0 };
    double scale = 1.0;
};

// Turns raw TouchEvents into per-step TouchGestures independent of the
// windowing backend.  Whenever the set of contacts changes (finger added
// or lifted) the reference is re-based and no gesture is reported for
// that step, so going from two fingers to one does not make the view jump.
class TouchGestureTracker {
  public:
    // Contact id -> position, sorted by id.
    using Contacts = std::flat_map<int, zipper::Vector<double, 2>>;

    auto update(const TouchEvent &e) -> std::optional<TouchGesture> {
        if (e.type == TouchEvent::Type::Cancel) {
            reset();
            return std::nullopt;
        }

        Contacts current;
        for (const auto &p : e.points) {
            if (p.state != TouchPoint::State::Released) {
                current.emplace(p.id, zipper::Vector<double, 2>{ p.x, p.y });
            }
        }

        std::optional<TouchGesture> gesture;
        if (!current.empty() && same_contacts(current)) {
            const auto [c0, s0] = centroid_and_spread(_points);
            const auto [c1, s1] = centroid_and_spread(current);
            TouchGesture g;
            g.touch_count = static_cast<int>(current.size());
            g.translation = c1 - c0;
            if (current.size() > 1 && s0 > min_spread) {
                g.scale = std::max(s1, min_spread) / s0;
            }
            gesture = g;
        }
        _points = std::move(current);
        return gesture;
    }

    void reset() { _points.clear(); }

    auto touch_count() const -> int { return static_cast<int>(_points.size()); }

    // Spreads below this (logical pixels) are too noisy to derive a scale from.
    static constexpr double min_spread = 1.0;

  private:
    Contacts _points;

    auto same_contacts(const Contacts &other) const -> bool {
        return std::ranges::equal(std::views::keys(other), std::views::keys(_points));
    }

    // Centroid and mean distance of the contacts to it.
    static auto centroid_and_spread(const Contacts &pts)
      -> std::pair<zipper::Vector<double, 2>, double> {
        zipper::Vector<double, 2> c{ 0.0, 0.0 };
        for (const auto &[id, p] : pts) c = c + p;
        c = c / static_cast<double>(pts.size());
        double spread = 0.0;
        for (const auto &[id, p] : pts) spread += (p - c).norm();
        return { c, spread / static_cast<double>(pts.size()) };
    }
};

}// namespace balsa::visualization
