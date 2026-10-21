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

inline auto always_true() -> bool { return true; }

struct BoolInput {
    Getter<bool> get{};
    Setter<bool> set{};
};

struct FloatInput {
    Getter<float> get{};
    Setter<float> set{};
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.01f;
    int display_decimals = 3;
    // TODO: do we only want these two options?
    bool use_slider = false; ///< slider instead of a spin/drag box
};

struct Vec3Input {
    Getter<scene_graph::Vec3f> get{};
    Setter<scene_graph::Vec3f> set{};
    float min = std::numeric_limits<float>::lowest();
    float max = std::numeric_limits<float>::max();
    float step = 0.1f;
    int display_decimals = 3;
};

/// RGBA colour in [0, 1].  With show_alpha == false the backend hides the
/// alpha channel; the value is still a Vec4f.
struct ColorInput {
    Getter<scene_graph::Vec4f> get{};
    Setter<scene_graph::Vec4f> set{};
    bool show_alpha = true;
};

/// Choice from a list, selected by index into options().  options() is
/// re-evaluated on refresh so the list can be dynamic (e.g. a mesh's
/// attribute names).
struct ChoiceInput {
    Getter<std::vector<std::string>> options{};
    Getter<int> get_index{};
    Setter<int> set_index{};
    std::function<bool(int)> option_enabled = [](int) -> bool { return true; };
};

struct TextInput {
    Getter<std::string> get{};
    Setter<std::string> set{};
};

/// Read-only text (counts, status).
struct ReadOnlyText {
    Getter<std::string> get{};
};

/// A button.
struct Button {
    std::function<void()> on_click{};
};

/// One labelled control: what it shows/edits is the widget alternative.
struct Property {
    std::string label{};
    std::string tooltip{};
    /// Tooltip that depends on live state; when set, it replaces tooltip
    /// and is re-evaluated on every refresh.
    Getter<std::string> live_tooltip{};
    Predicate enabled_when = always_true; ///< greyed out when false
    Predicate visible_when = always_true; ///< hidden when false
    std::variant<BoolInput,
                 FloatInput,
                 Vec3Input,
                 ColorInput,
                 ChoiceInput,
                 TextInput,
                 ReadOnlyText,
                 Button>
        widget;
};

} // namespace balsa::visualization::controls
