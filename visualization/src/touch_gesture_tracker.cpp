#include "balsa/visualization/touch_gesture_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
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
    // Mean change in angle about each step's centroid; contacts too close
    // to the centroid have no meaningful angle and are skipped.
    auto mean_rotation(const TouchGestureTracker::Contacts &before,
                       const TouchGestureTracker::Contacts &after,
                       const zipper::Vector<double, 2> &c0,
                       const zipper::Vector<double, 2> &c1) -> double {
        double sum = 0.0;
        int n = 0;
        for (const auto &[id, p1] : after) {
            const auto a = before.at(id) - c0;
            const auto b = p1 - c1;
            if (a.norm() < TouchGestureTracker::min_spread
                || b.norm() < TouchGestureTracker::min_spread) {
                continue;
            }
            // Screen y points down, so this is clockwise-positive.
            double d = std::atan2(b(1), b(0)) - std::atan2(a(1), a(0));
            d = std::remainder(d, 2.0 * std::numbers::pi);
            sum += d;
            ++n;
        }
        return n > 0 ? sum / n : 0.0;
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
        g.centroid = c1;
        g.translation = c1 - c0;
        if (current.size() > 1 && s0 > min_spread) {
            g.scale = std::max(s1, min_spread) / s0;
        }
        if (current.size() > 1) {
            g.rotation = mean_rotation(_points, current, c0, c1);
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
