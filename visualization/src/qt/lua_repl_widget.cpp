// Qt defines 'emit' as a macro, which conflicts with TBB's
// tbb::profiling::emit().  Undefine it before pulling in headers
// that transitively include TBB (via quiver), then restore it.
#undef emit

#include "balsa/visualization/qt/lua_repl_widget.hpp"
#include "balsa/lua/lua_repl.hpp"

#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollBar>
#include <QVBoxLayout>

// Restore Qt's emit macro.
#define emit

namespace balsa::visualization::qt {

LuaReplWidget::LuaReplWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // ── Output area ─────────────────────────────────────────────────
    _output = new QPlainTextEdit(this);
    _output->setReadOnly(true);
    _output->setUndoRedoEnabled(false);

    // Use monospace font for output.
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(10);
    _output->setFont(mono);

    layout->addWidget(_output, /*stretch=*/1);

    // ── Input row ───────────────────────────────────────────────────
    auto *input_layout = new QHBoxLayout();
    input_layout->setContentsMargins(0, 0, 0, 0);

    _input = new QLineEdit(this);
    _input->setPlaceholderText("Enter Lua code...");
    _input->setFont(mono);
    input_layout->addWidget(_input, /*stretch=*/1);

    _clear_btn = new QPushButton("Clear", this);
    input_layout->addWidget(_clear_btn);

    layout->addLayout(input_layout);

    // ── Connections ─────────────────────────────────────────────────
    connect(_input,
            &QLineEdit::returnPressed,
            this,
            &LuaReplWidget::execute_command);
    connect(_clear_btn, &QPushButton::clicked, this, [this]() {
        if (_repl) {
            _repl->clear_output();
            _output->clear();
        }
    });

    // Install event filter for Up/Down history navigation.
    _input->installEventFilter(this);
}

auto LuaReplWidget::set_repl(balsa::lua::LuaRepl *repl) -> void { _repl = repl; }

auto LuaReplWidget::execute_command() -> void {
    if (!_repl) return;

    QString text = _input->text().trimmed();
    if (text.isEmpty()) return;

    _repl->execute(text.toStdString());
    _input->clear();
    _history_index = -1;

    refresh_output();
    emit command_executed();
}

auto LuaReplWidget::refresh_output() -> void {
    if (!_repl) return;

    _output->setPlainText(QString::fromStdString(_repl->output()));

    // Scroll to bottom.
    auto *sb = _output->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

auto LuaReplWidget::eventFilter(QObject *obj, QEvent *event) -> bool {
    if (obj == _input && event->type() == QEvent::KeyPress && _repl) {
        auto *ke = static_cast<QKeyEvent *>(event);
        const auto &history = _repl->history();

        if (ke->key() == ::Qt::Key_Up && !history.empty()) {
            if (_history_index < 0) {
                _history_index = static_cast<int>(history.size()) - 1;
            } else if (_history_index > 0) {
                _history_index--;
            }
            _input->setText(QString::fromStdString(
                history[static_cast<std::size_t>(_history_index)]));
            return true;
        }

        if (ke->key() == ::Qt::Key_Down && !history.empty()) {
            if (_history_index >= 0) {
                _history_index++;
                if (_history_index >= static_cast<int>(history.size())) {
                    _history_index = -1;
                }
            }
            if (_history_index >= 0) {
                _input->setText(QString::fromStdString(
                    history[static_cast<std::size_t>(_history_index)]));
            } else {
                _input->clear();
            }
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

} // namespace balsa::visualization::qt
