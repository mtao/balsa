#pragma once
#include <QString>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "balsa/visualization/controls/Panel.hpp"

class QBoxLayout;
class QFormLayout;

namespace balsa::visualization::qt {

class VerticalTabWidget;

// Qt frontend for the controls model: one vertical tab per registered Tab
// that has at least one applicable Panel, one collapsible box per Panel.
//
// Widgets are built when the context (scene / selection) changes or when
// the set of applicable panels changes; otherwise edits only re-read values.
class ControlsWidget : public QWidget {
    Q_OBJECT

  public:
    explicit ControlsWidget(QWidget *parent = nullptr);
    ~ControlsWidget() override;

    void set_scene(::balsa::visualization::vulkan::MeshScene *scene);
    void set_selected_object(::balsa::scene_graph::Object *obj);

    // Re-read every value from the model (call after external changes).
    void refresh();

  signals:
    void scene_changed();

  private:
    void rebuild();
    void after_edit();
    auto applicable_panels() const -> std::vector<std::string>;

    auto build_panel(const controls::Panel &panel) -> QWidget *;
    void build_layout(const controls::Layout &layout, QWidget *parent,
                      QBoxLayout *into);
    auto build_property(const controls::Property &prop, QWidget *parent)
        -> QWidget *;

    controls::PanelContext m_context;
    VerticalTabWidget *m_tabs = nullptr;
    QBoxLayout *m_root = nullptr;
    std::string m_current_tab;
    std::vector<std::string> m_panel_signature;
    std::map<std::string, bool> m_panel_open;

    // Keep layouts alive: properties' accessors live inside them.
    std::vector<std::unique_ptr<controls::Layout>> m_layouts;
    std::vector<std::function<void()>> m_refreshers;
    bool m_refreshing = false;
};

} // namespace balsa::visualization::qt
