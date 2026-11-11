#include "balsa/visualization/touch_gesture_tracker.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

namespace balsa::visualization {

namespace {
    // Centroid and mean distance of the contacts to it.
    auto centroid_and_spread(const TouchGestureTracker::Contacts &pts)
      -> std::pair<zipper::Vector<double, 2>, double> {
        zipper::Vector<double, 2> c{ 0.0, 0.0 };
        for (const auto &[id, p] : pts) c = c + p;
        c = c / static_cast<double>(pts.size());
        double spread = 0.0;
        for (const auto &[id, p] : pts) spread += (p - c).norm();
        return { c, spread / static_cast<double>(pts.size()) };
    }
}// namespace

auto TouchGestureTracker::update(const TouchEvent &e) -> std::optional<TouchGesture> {
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
    if (!current.empty()
        && std::ranges::equal(std::views::keys(current), std::views::keys(_points))) {
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

auto TouchGestureTracker::reset() -> void { _points.clear(); }

auto TouchGestureTracker::touch_count() const -> int {
    return static_cast<int>(_points.size());
}

}// namespace balsa::visualization
