#if !defined(BALSA_VISUALIZATION_QT_VERTICAL_TAB_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_VERTICAL_TAB_WIDGET_HPP

#include <QTabBar>
#include <QWidget>

class QStackedWidget;
class QScrollArea;

namespace balsa::visualization::qt {

// ── VerticalTabBar ──────────────────────────────────────────────────
//
// A QTabBar subclass that draws tabs vertically along the left edge,
// with horizontal text and an icon above each label (Blender/VS Code
// style).  Each tab cell is ~70 px wide x ~64 px tall.

class VerticalTabBar : public QTabBar {
    Q_OBJECT

  public:
    explicit VerticalTabBar(QWidget *parent = nullptr);
    ~VerticalTabBar() override;

  protected:
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;
    void paintEvent(QPaintEvent *event) override;
};

// ── VerticalTabWidget ───────────────────────────────────────────────
//
// Composite widget that pairs a VerticalTabBar on the left with a
// QStackedWidget on the right.  Each page is automatically wrapped
// in a QScrollArea.  Tabs can be shown/hidden and enabled/disabled
// per-index.

class VerticalTabWidget : public QWidget {
    Q_OBJECT

  public:
    explicit VerticalTabWidget(QWidget *parent = nullptr);
    ~VerticalTabWidget() override;

    // Add a tab with an icon character (Unicode) and label.
    // Returns the tab index.  `page` is reparented into a QScrollArea.
    int addTab(QWidget *page, const QString &icon_char, const QString &label);

    // Access the page widget (the original widget, not the scroll area).
    QWidget *widget(int index) const;

    // Current tab index.
    int currentIndex() const;
    void setCurrentIndex(int index);

    // Per-tab visibility (hides both the tab button and the page).
    void setTabVisible(int index, bool visible);

    // Per-tab enabled state.
    void setTabEnabled(int index, bool enabled);

  signals:
    void currentChanged(int index);

  private:
    VerticalTabBar *_tab_bar = nullptr;
    QStackedWidget *_stack = nullptr;

    // Map from our logical index to the page widget.
    // (We need this because hiding tabs in QTabBar shifts indices.)
    struct TabEntry {
        QWidget *page = nullptr;
        QScrollArea *scroll = nullptr;
    };
    std::vector<TabEntry> _tabs;
};

}// namespace balsa::visualization::qt

#endif
