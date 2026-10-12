#include "balsa/visualization/qt/ControlsWidget.hpp"
#include "balsa/visualization/qt/vertical_tab_widget.hpp"
#include "balsa/visualization/qt/zipper_compat.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace balsa::visualization::qt {

namespace ctl = controls;
namespace sg = scene_graph;

namespace {

auto qstr(const std::string &s) -> QString { return QString::fromStdString(s); }

void set_button_color(QPushButton *btn, const sg::Vec4f &c) {
    btn->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid gray;")
            .arg(to_qcolor(c).name(QColor::HexArgb)));
}

auto make_spin(QWidget *parent, double min, double max, double step,
               int decimals) -> QDoubleSpinBox * {
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setKeyboardTracking(false);
    s->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    return s;
}

// Sliders are integer; map [min, max] onto this many steps.
constexpr int k_slider_steps = 1000;

void fill_combo(QComboBox *combo, const ctl::ChoiceInput &choice) {
    const auto options = choice.options();
    bool same = combo->count() == static_cast<int>(options.size());
    for (int i = 0; same && i < combo->count(); ++i) {
        same = combo->itemText(i) == qstr(options[static_cast<std::size_t>(i)]);
    }
    if (!same) {
        combo->clear();
        for (const auto &s : options) combo->addItem(qstr(s));
    }
    if (auto *model = qobject_cast<QStandardItemModel *>(combo->model())) {
        for (int i = 0; i < model->rowCount(); ++i) {
            model->item(i)->setEnabled(choice.option_enabled(i));
        }
    }
    combo->setCurrentIndex(choice.get_index());
}

} // namespace

ControlsWidget::ControlsWidget(QWidget *parent) : QWidget(parent) {
    ctl::register_builtin_panels();
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(0, 0, 0, 0);
    rebuild();
}

ControlsWidget::~ControlsWidget() = default;

void ControlsWidget::set_scene(vulkan::MeshScene *scene) {
    m_context.scene = scene;
    rebuild();
}

void ControlsWidget::set_selected_object(sg::Object *obj) {
    m_context.active_object = obj;
    rebuild();
}

auto ControlsWidget::applicable_panels() const -> std::vector<std::string> {
    std::vector<std::string> ids;
    for (const auto *tab : ctl::registered_tabs()) {
        for (const auto *p : ctl::available_panels(m_context, tab->id)) {
            ids.push_back(p->id);
        }
    }
    return ids;
}

void ControlsWidget::refresh() {
    m_refreshing = true;
    for (const auto &r : m_refreshers) r();
    m_refreshing = false;
}

void ControlsWidget::after_edit() {
    if (m_refreshing) return;
    emit scene_changed();
    if (applicable_panels() != m_panel_signature) {
        // Defer: we're inside a signal of a widget the rebuild deletes.
        QTimer::singleShot(0, this, [this] { rebuild(); });
    } else {
        refresh();
    }
}

void ControlsWidget::rebuild() {
    if (m_tabs) {
        // Old widgets reference the layouts we're about to drop; silence
        // them (e.g. editingFinished on focus loss) before tearing down.
        for (auto *w : m_tabs->findChildren<QWidget *>()) w->blockSignals(true);
        m_tabs->hide();
        m_tabs->deleteLater();
    }
    m_refreshers.clear();
    m_layouts.clear();
    m_tabs = new VerticalTabWidget(this);
    m_root->addWidget(m_tabs);
    m_panel_signature = applicable_panels();

    std::vector<std::string> tab_ids;
    int current = -1;
    for (const auto *tab : ctl::registered_tabs()) {
        const auto panels = ctl::available_panels(m_context, tab->id);
        if (panels.empty()) continue;

        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(4, 4, 4, 4);
        for (const auto *panel : panels) layout->addWidget(build_panel(*panel));
        layout->addStretch();

        if (tab->id == m_current_tab) current = static_cast<int>(tab_ids.size());
        tab_ids.push_back(tab->id);
        m_tabs->addTab(page, qstr(tab->icon_glyph), qstr(tab->label));
    }
    if (current >= 0) m_tabs->setCurrentIndex(current);
    connect(m_tabs, &VerticalTabWidget::currentChanged, this,
            [this, tab_ids](int i) {
                if (i >= 0 && i < static_cast<int>(tab_ids.size())) {
                    m_current_tab = tab_ids[static_cast<std::size_t>(i)];
                }
            });
    refresh();
}

auto ControlsWidget::build_panel(const ctl::Panel &panel) -> QWidget * {
    auto &layout = *m_layouts.emplace_back(std::make_unique<ctl::Layout>());
    panel.fill_layout(m_context, layout);

    auto *box = new QFrame;
    box->setFrameShape(QFrame::StyledPanel);
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(4, 2, 4, 4);

    auto *header = new QToolButton(box);
    header->setText(qstr(panel.label).replace("&", "&&"));
    header->setCheckable(true);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setAutoRaise(true);
    header->setStyleSheet("QToolButton { font-weight: bold; border: none; }");
    v->addWidget(header);

    auto *content = new QWidget(box);
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    build_layout(layout, content, cl);
    v->addWidget(content);

    auto [it, inserted] = m_panel_open.try_emplace(panel.id, panel.expanded_by_default);
    auto apply = [header, content](bool open) {
        header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(open);
    };
    header->setChecked(it->second);
    apply(it->second);
    connect(header, &QToolButton::toggled, this,
            [this, id = panel.id, apply](bool open) {
                m_panel_open[id] = open;
                apply(open);
            });
    return box;
}

void ControlsWidget::build_layout(const ctl::Layout &layout, QWidget *parent,
                                  QBoxLayout *into) {
    const bool row = layout.direction() == ctl::Layout::Direction::Row;
    QFormLayout *form = nullptr;
    QHBoxLayout *hbox = nullptr;

    QWidget *group = parent;
    if (!layout.heading().empty() && !row) {
        group = new QWidget(parent);
        auto *gl = new QVBoxLayout(group);
        gl->setContentsMargins(0, 4, 0, 0);
        auto *title = new QLabel(qstr(layout.heading()), group);
        title->setStyleSheet("font-weight: bold;");
        gl->addWidget(title);
        into->addWidget(group);
        into = gl;
    }
    if (row) {
        auto *w = new QWidget(parent);
        hbox = new QHBoxLayout(w);
        hbox->setContentsMargins(0, 0, 0, 0);
        if (!layout.heading().empty()) {
            auto *h = new QLabel(qstr(layout.heading()), w);
            h->setMinimumWidth(70);
            hbox->addWidget(h);
        }
        into->addWidget(w);
        group = w;
    } else {
        form = new QFormLayout;
        form->setContentsMargins(0, 0, 0, 0);
        into->addLayout(form);
    }
    m_refreshers.push_back([group, &layout, parent] {
        if (group != parent) group->setVisible(layout.visible_when()());
    });

    for (const auto &item : layout.items()) {
        if (const auto *child = std::get_if<std::unique_ptr<ctl::Layout>>(&item)) {
            if (form) {
                auto *holder = new QWidget(group);
                auto *hl = new QVBoxLayout(holder);
                hl->setContentsMargins(0, 0, 0, 0);
                build_layout(**child, holder, hl);
                form->addRow(holder);
            } else {
                build_layout(**child, group, hbox);
            }
            continue;
        }
        const auto &prop = std::get<ctl::Property>(item);
        QWidget *w = build_property(prop, group);
        w->setToolTip(qstr(prop.tooltip));

        // Bools and actions carry their own text.
        const bool self_labelled =
            std::holds_alternative<ctl::BoolInput>(prop.widget)
            || std::holds_alternative<ctl::Button>(prop.widget);
        QLabel *label = nullptr;
        if (form) {
            if (self_labelled) {
                form->addRow(w);
            } else {
                label = new QLabel(qstr(prop.label), group);
                form->addRow(label, w);
            }
        } else {
            if (!self_labelled) {
                label = new QLabel(qstr(prop.label), group);
                hbox->addWidget(label);
            }
            const bool stretch =
                std::holds_alternative<ctl::FloatInput>(prop.widget)
                || std::holds_alternative<ctl::Vec3Input>(prop.widget)
                || std::holds_alternative<ctl::ChoiceInput>(prop.widget)
                || std::holds_alternative<ctl::TextInput>(prop.widget);
            hbox->addWidget(w, stretch ? 1 : 0);
        }
        m_refreshers.push_back([w, label, &prop] {
            const bool visible = prop.visible_when();
            w->setVisible(visible);
            if (label) label->setVisible(visible);
            w->setEnabled(prop.enabled_when());
        });
    }
    if (hbox) hbox->addStretch(0);
}

auto ControlsWidget::build_property(const ctl::Property &prop, QWidget *parent)
    -> QWidget * {
    return std::visit(
        [&](const auto &v) -> QWidget * {
            using V = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<V, ctl::BoolInput>) {
                auto *c = new QCheckBox(qstr(prop.label), parent);
                connect(c, &QCheckBox::toggled, this, [this, &v](bool b) {
                    v.set(b);
                    after_edit();
                });
                m_refreshers.push_back([c, &v] {
                    QSignalBlocker b(c);
                    c->setChecked(v.get());
                });
                return c;

            } else if constexpr (std::is_same_v<V, ctl::FloatInput>) {
                if (!v.use_slider) {
                    auto *s = make_spin(parent, v.min, v.max, v.step, v.display_decimals);
                    connect(s, &QDoubleSpinBox::valueChanged, this,
                            [this, &v](double x) {
                                v.set(static_cast<float>(x));
                                after_edit();
                            });
                    m_refreshers.push_back([s, &v] {
                        QSignalBlocker b(s);
                        s->setValue(v.get());
                    });
                    return s;
                }
                auto *w = new QWidget(parent);
                auto *h = new QHBoxLayout(w);
                h->setContentsMargins(0, 0, 0, 0);
                auto *s = new QSlider(Qt::Horizontal, w);
                s->setRange(0, k_slider_steps);
                auto *value = new QLabel(w);
                value->setMinimumWidth(40);
                h->addWidget(s, 1);
                h->addWidget(value);
                auto to_float = [&v](int i) {
                    return v.min + (v.max - v.min) * static_cast<float>(i)
                                       / k_slider_steps;
                };
                connect(s, &QSlider::valueChanged, this,
                        [this, &v, to_float](int i) {
                            v.set(to_float(i));
                            after_edit();
                        });
                m_refreshers.push_back([s, value, &v] {
                    QSignalBlocker b(s);
                    const float x = v.get();
                    s->setValue(static_cast<int>(std::lround(
                        (x - v.min) / (v.max - v.min) * k_slider_steps)));
                    value->setText(QString::number(x, 'f', 2));
                });
                return w;

            } else if constexpr (std::is_same_v<V, ctl::Vec3Input>) {
                auto *w = new QWidget(parent);
                auto *h = new QHBoxLayout(w);
                h->setContentsMargins(0, 0, 0, 0);
                std::array<QDoubleSpinBox *, 3> spins{};
                for (auto &s : spins) {
                    s = make_spin(w, v.min, v.max, v.step, v.display_decimals);
                    h->addWidget(s, 1);
                }
                for (auto *s : spins) {
                    connect(s, &QDoubleSpinBox::valueChanged, this,
                            [this, &v, spins] {
                                v.set(read_vec3(spins[0], spins[1], spins[2]));
                                after_edit();
                            });
                }
                m_refreshers.push_back([spins, &v] {
                    const auto x = v.get();
                    for (int i = 0; i < 3; ++i) {
                        QSignalBlocker b(spins[static_cast<std::size_t>(i)]);
                        spins[static_cast<std::size_t>(i)]->setValue(x(i));
                    }
                });
                return w;

            } else if constexpr (std::is_same_v<V, ctl::ColorInput>) {
                auto *b = new QPushButton(parent);
                b->setFixedWidth(48);
                connect(b, &QPushButton::clicked, this, [this, b, &v, &prop] {
                    auto opts = v.show_alpha ? QColorDialog::ShowAlphaChannel
                                        : QColorDialog::ColorDialogOptions{};
                    QColor c = QColorDialog::getColor(to_qcolor(v.get()), this,
                                                      qstr(prop.label), opts);
                    if (!c.isValid()) return;
                    v.set(as_zipper(c));
                    set_button_color(b, v.get());
                    after_edit();
                });
                m_refreshers.push_back([b, &v] { set_button_color(b, v.get()); });
                return b;

            } else if constexpr (std::is_same_v<V, ctl::ChoiceInput>) {
                auto *c = new QComboBox(parent);
                connect(c, &QComboBox::currentIndexChanged, this,
                        [this, &v](int i) {
                            if (i < 0) return;
                            v.set_index(i);
                            after_edit();
                        });
                m_refreshers.push_back([c, &v] {
                    QSignalBlocker b(c);
                    fill_combo(c, v);
                });
                return c;

            } else if constexpr (std::is_same_v<V, ctl::TextInput>) {
                auto *e = new QLineEdit(parent);
                connect(e, &QLineEdit::editingFinished, this, [this, e, &v] {
                    v.set(e->text().toStdString());
                    after_edit();
                });
                m_refreshers.push_back([e, &v] {
                    if (e->hasFocus()) return; // don't clobber typing
                    QSignalBlocker b(e);
                    e->setText(qstr(v.get()));
                });
                return e;

            } else if constexpr (std::is_same_v<V, ctl::ReadOnlyText>) {
                auto *l = new QLabel(parent);
                m_refreshers.push_back([l, &v] { l->setText(qstr(v.get())); });
                return l;

            } else {
                static_assert(std::is_same_v<V, ctl::Button>);
                auto *b = new QPushButton(qstr(prop.label), parent);
                connect(b, &QPushButton::clicked, this, [this, &v] {
                    v.on_click();
                    after_edit();
                });
                return b;
            }
        },
        prop.widget);
}

} // namespace balsa::visualization::qt
