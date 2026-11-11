#include "balsa/scene_graph/DebugView.hpp"

#include <algorithm>

namespace balsa::scene_graph {

auto DebugView::option_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(m_channels.size() + 1);
    names.emplace_back(m_output_label);
    for (const auto name : m_channels) { names.emplace_back(name); }
    return names;
}

auto DebugView::channel_name() const -> std::string_view {
    return m_channel == 0 ? m_output_label : m_channels[m_channel - 1];
}

auto DebugView::set_channel(std::size_t channel) -> bool {
    if (channel > m_channels.size()) { return false; }
    m_channel = channel;
    return true;
}

auto DebugView::set_channel(std::string_view name) -> bool {
    if (name == m_output_label) {
        m_channel = 0;
        return true;
    }
    const auto it = std::ranges::find(m_channels, name);
    if (it == m_channels.end()) { return false; }
    m_channel = static_cast<std::size_t>(it - m_channels.begin()) + 1;
    return true;
}

auto debug_view_property(DebugView &view, std::string label)
  -> visualization::controls::Property {
    using namespace visualization::controls;
    return {.label = std::move(label),
            .tooltip = "Show one input of the shading instead of the output",
            .widget = ChoiceInput{
              .options = [&view] { return view.option_names(); },
              .get_index = [&view] { return static_cast<int>(view.channel()); },
              .set_index =
                [&view](const int &i) {
                    if (i >= 0) { view.set_channel(static_cast<std::size_t>(i)); }
                }}};
}

} // namespace balsa::scene_graph
