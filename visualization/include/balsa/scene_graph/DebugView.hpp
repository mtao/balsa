#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "balsa/visualization/controls/Property.hpp"

namespace balsa::scene_graph {

/// What a drawable shows: its normal output, or one of its named debug
/// channels (an input or intermediate of its shading, such as a normal or a
/// colour channel).
///
/// Channel 0 is the normal output; channel i > 0 is channels()[i - 1].  The
/// channel index is what the drawable's shader receives, so a shader lists
/// its channels in the same order.  The names are not owned: they must
/// outlive the view (typically static arrays).
class DebugView {
  public:
    DebugView() = default;
    DebugView(std::string_view output_label, std::span<const std::string_view> channels)
      : m_output_label(output_label), m_channels(channels) {}

    /// The name of channel 0, the normal output (e.g. "Shaded", "RGBA").
    [[nodiscard]] auto output_label() const -> std::string_view { return m_output_label; }
    /// The debug channels, channel 1 onwards.
    [[nodiscard]] auto channels() const -> std::span<const std::string_view> {
        return m_channels;
    }
    /// output_label() then channels().
    [[nodiscard]] auto option_names() const -> std::vector<std::string>;

    [[nodiscard]] auto channel() const -> std::size_t { return m_channel; }
    /// The name of the current channel.
    [[nodiscard]] auto channel_name() const -> std::string_view;
    /// False when showing the normal output.
    [[nodiscard]] auto active() const -> bool { return m_channel != 0; }
    /// Selects channel; false (and unchanged) if out of range.
    auto set_channel(std::size_t channel) -> bool;
    /// Selects the channel named name (or output_label()); false if none.
    auto set_channel(std::string_view name) -> bool;
    auto reset() -> void { m_channel = 0; }

  private:
    std::string_view m_output_label = "Output";
    std::span<const std::string_view> m_channels;
    std::size_t m_channel = 0;
};

/// A choice control over view's channels.  view must outlive the control.
[[nodiscard]] auto debug_view_property(DebugView &view, std::string label = "Debug View")
  -> visualization::controls::Property;

} // namespace balsa::scene_graph
