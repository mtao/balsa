#pragma once
#include <flat_map>
#include <optional>

#include <zipper/Vector.hpp>

#include "balsa/visualization/input_handler.hpp"

namespace balsa::visualization {

// Incremental motion of a set of touch contacts between two TouchEvents.
//
//   touch_count  number of contacts that stayed down for the whole step
//   centroid     their centroid after the step (logical pixels)
//   translation  movement of the centroid during the step
//   scale        change in spread (>1 = fingers moving apart); always 1
//                for a single contact
//   rotation     mean turn of the contacts about the centroid (radians,
//                clockwise on screen); always 0 for a single contact
struct TouchGesture {
    int touch_count = 0;
    zipper::Vector<double, 2> centroid{ 0.0, 0.0 };
    zipper::Vector<double, 2> translation{ 0.0, 0.0 };
    double scale = 1.0;
    double rotation = 0.0;
};

// Turns raw TouchEvents into per-step TouchGestures independent of the
// windowing backend.  Whenever the set of contacts changes (finger added
// or lifted) the reference is re-based and no gesture is reported for
// that step, so going from two fingers to one does not make the view jump.
class TouchGestureTracker {
  public:
    // Contact id -> position, sorted by id.
    using Contacts = std::flat_map<int, zipper::Vector<double, 2>>;

    auto update(const TouchEvent &e) -> std::optional<TouchGesture>;
    auto reset() -> void;
    auto touch_count() const -> int;

    // Spreads below this (logical pixels) are too noisy to derive a scale from.
    static constexpr double min_spread = 1.0;

  private:
    Contacts _points;
};

}// namespace balsa::visualization
