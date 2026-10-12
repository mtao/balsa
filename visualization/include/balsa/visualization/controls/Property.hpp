#pragma once

#include <functional>
#include <limits>
#include <string>
#include <variant>
#include <vector>

#include "balsa/scene_graph/types.hpp"

namespace balsa::visualization::controls {

template <typename T>
using Getter = std::function<T()>;
template <typename T>
using Setter = std::function<void(const T &)>;
using Predicate = std::function<bool()>;

inline auto always() -> bool { return true; }

struct BoolValue {
    Getter<bool> get{};
    Setter<bool> set{};
};

struct FloatValue {
    Getter<float> get{};
    Setter<float> set{};
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.01f;
    int decimals = 3;
    // TODO: do we only want these two options?
    bool slider = false; ///< slider instead of a spin/drag box
};

struct Vec3Value {
    Getter<scene_graph::Vec3f> get{};
    Setter<scene_graph::Vec3f> set{};
    float min = std::numeric_limits<float>::lowest();
    float max = std::numeric_limits<float>::max();
    float step = 0.1f;
    int decimals = 3;
};

/// RGBA colour in [0, 1].  With alpha == false the backend hides the alpha
/// channel; the value is still a Vec4f.
struct ColorValue {
    Getter<scene_graph::Vec4f> get{};
    Setter<scene_graph::Vec4f> set{};
    bool alpha = true;
};

/// Choice from a list.  items() is re-evaluated on refresh so the list can
/// be dynamic (e.g. a mesh's attribute names).
struct EnumValue {
    Getter<std::vector<std::string>> items{};
    Getter<int> get{};
    Setter<int> set{};
    std::function<bool(int)> item_enabled = [](int) -> bool { return true; };
};

struct StringValue {
    Getter<std::string> get{};
    Setter<std::string> set{};
};

/// Read-only text (counts, status).
struct LabelValue {
    Getter<std::string> get{};
};

/// A button.
struct ActionValue {
    std::function<void()> run{};
};

struct Property {
    std::string label{};
    std::string tooltip{};
    Predicate enabled = always; ///< greyed out when false
    Predicate visible = always; ///< hidden when false
    std::variant<BoolValue,
                 FloatValue,
                 Vec3Value,
                 ColorValue,
                 EnumValue,
                 StringValue,
                 LabelValue,
                 ActionValue>
        value;
};

} // namespace balsa::visualization::controls
