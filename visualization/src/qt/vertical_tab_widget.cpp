#include "balsa/visualization/qt/vertical_tab_widget.hpp"

#include <QBoxLayout>
#include <QPainter>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyleOptionTab>

namespace balsa::visualization::qt {

// ═══════════════════════════════════════════════════════════════════════
// VerticalTabBar
// ═══════════════════════════════════════════════════════════════════════

static constexpr int k_tab_width = 70;
static constexpr int k_tab_height = 64;
static constexpr int k_icon_font_size = 20;
static constexpr int k_label_font_size = 9;

VerticalTabBar::VerticalTabBar(QWidget *parent) : QTabBar(parent) {
    setShape(QTabBar::RoundedWest);
    setDrawBase(false);
    setExpanding(false);
    setElideMode(Qt::ElideNone);
    setUsesScrollButtons(true);
}

VerticalTabBar::~VerticalTabBar() = default;

QSize VerticalTabBar::tabSizeHint(int /*index*/) const {
    return { k_tab_width, k_tab_height };
}

QSize VerticalTabBar::minimumTabSizeHint(int index) const {
    return tabSizeHint(index);
}

void VerticalTabBar::paintEvent(QPaintEvent * /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    for (int i = 0; i < count(); ++i) {
        if (!isTabVisible(i)) continue;

        QRect rect = tabRect(i);
        bool selected = (i == currentIndex());
        bool enabled = isTabEnabled(i);

        // Background
        if (selected) {
            p.fillRect(rect, palette().window());
            // Left accent bar (3 px)
            p.fillRect(QRect(rect.left(), rect.top(), 3, rect.height()),
                       palette().highlight());
        } else {
            p.fillRect(rect, palette().midlight());
        }

        // Text color
        QColor text_color = enabled
                              ? (selected ? palette().text().color() : palette().windowText().color())
                              : palette().color(QPalette::Disabled, QPalette::WindowText);
        p.setPen(text_color);

        // Icon character (upper portion of the cell)
        QString icon_char = tabData(i).toString();
        if (!icon_char.isEmpty()) {
            QFont icon_font = p.font();
            icon_font.setPixelSize(k_icon_font_size);
            p.setFont(icon_font);
            QRect icon_rect(rect.left(), rect.top() + 6, rect.width(), rect.height() / 2);
            p.drawText(icon_rect, Qt::AlignHCenter | Qt::AlignTop, icon_char);
        }

        // Label (lower portion)
        QFont label_font = p.font();
        label_font.setPixelSize(k_label_font_size);
        p.setFont(label_font);
        QRect label_rect(rect.left(), rect.top() + rect.height() / 2, rect.width(), rect.height() / 2 - 4);
        p.drawText(label_rect, Qt::AlignHCenter | Qt::AlignTop, tabText(i));

        // Subtle bottom separator
        p.setPen(palette().mid().color());
        p.drawLine(rect.bottomLeft(), rect.bottomRight());
    }
}

// ═══════════════════════════════════════════════════════════════════════
// VerticalTabWidget
// ═══════════════════════════════════════════════════════════════════════

VerticalTabWidget::VerticalTabWidget(QWidget *parent) : QWidget(parent) {
    _tab_bar = new VerticalTabBar(this);
    _stack = new QStackedWidget(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_tab_bar, 0);
    layout->addWidget(_stack, 1);

    connect(_tab_bar, &QTabBar::currentChanged, this, [this](int idx) {
        if (idx >= 0 && idx < static_cast<int>(_tabs.size())) {
            _stack->setCurrentIndex(idx);
        }
        emit currentChanged(idx);
    });
}

VerticalTabWidget::~VerticalTabWidget() = default;

int VerticalTabWidget::addTab(QWidget *page, const QString &icon_char, const QString &label) {
    // Wrap the page in a QScrollArea
    auto *scroll = new QScrollArea(_stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    page->setParent(scroll);

    int idx = _tab_bar->addTab(label);
    _tab_bar->setTabData(idx, icon_char);
    _stack->addWidget(scroll);

    _tabs.push_back({ page, scroll });
    return idx;
}

QWidget *VerticalTabWidget::widget(int index) const {
    if (index < 0 || index >= static_cast<int>(_tabs.size())) return nullptr;
    return _tabs[static_cast<std::size_t>(index)].page;
}

int VerticalTabWidget::currentIndex() const {
    return _tab_bar->currentIndex();
}

void VerticalTabWidget::setCurrentIndex(int index) {
    _tab_bar->setCurrentIndex(index);
}

void VerticalTabWidget::setTabVisible(int index, bool visible) {
    _tab_bar->setTabVisible(index, visible);
    if (index >= 0 && index < static_cast<int>(_tabs.size())) {
        // If we're hiding the current tab, switch to the first visible one.
        if (!visible && _tab_bar->currentIndex() == index) {
            for (int i = 0; i < _tab_bar->count(); ++i) {
                if (_tab_bar->isTabVisible(i) && _tab_bar->isTabEnabled(i)) {
                    _tab_bar->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
}

void VerticalTabWidget::setTabEnabled(int index, bool enabled) {
    _tab_bar->setTabEnabled(index, enabled);
}

}// namespace balsa::visualization::qt
