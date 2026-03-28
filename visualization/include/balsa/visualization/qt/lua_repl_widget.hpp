#pragma once

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
    void set_repl(balsa::lua::LuaRepl *repl);

  signals:
    // Emitted after a command is executed, so the main window can
    // request a re-render.
    void command_executed();

  protected:
    // Handle Up/Down arrow keys in the input line for history nav.
    bool eventFilter(QObject *obj, QEvent *event) override;

  private:
    void execute_command();
    void refresh_output();

    balsa::lua::LuaRepl *_repl = nullptr;
    QPlainTextEdit *_output = nullptr;
    QLineEdit *_input = nullptr;
    QPushButton *_clear_btn = nullptr;

    int _history_index = -1;
};

} // namespace balsa::visualization::qt
