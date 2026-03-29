#if !defined(BALSA_VISUALIZATION_QT_LUA_REPL_WIDGET_HPP)
#define BALSA_VISUALIZATION_QT_LUA_REPL_WIDGET_HPP

// Qt Lua REPL widget — provides an interactive Lua console as a
// QWidget suitable for embedding in a QDockWidget.
//
// Pattern: QWidget subclass with Q_OBJECT, matching the existing
// MeshControlsWidget design.

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>

namespace balsa::lua {
class LuaRepl;
}

namespace balsa::visualization::qt {

class LuaReplWidget : public QWidget {
    Q_OBJECT

  public:
    explicit LuaReplWidget(QWidget *parent = nullptr);
    ~LuaReplWidget() override = default;

    // Set the REPL engine.  Must be called before the widget is used.
    // The LuaRepl must outlive the widget.
    auto set_repl(balsa::lua::LuaRepl *repl) -> void;

  signals:
    // Emitted after a command is executed, so the main window can
    // request a re-render.
    void command_executed();

  protected:
    // Handle Up/Down arrow keys in the input line for history nav.
    auto eventFilter(QObject *obj, QEvent *event) -> bool override;

  private:
    auto execute_command() -> void;
    auto refresh_output() -> void;

    balsa::lua::LuaRepl *_repl = nullptr;
    QPlainTextEdit *_output = nullptr;
    QLineEdit *_input = nullptr;
    QPushButton *_clear_btn = nullptr;

    int _history_index = -1;
};

} // namespace balsa::visualization::qt

#endif // BALSA_VISUALIZATION_QT_LUA_REPL_WIDGET_HPP
