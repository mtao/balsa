#pragma once

// LuaRepl — lightweight Lua REPL engine.
//
// Owns a sol::state, captures print() output, maintains command
// history, and provides execute() for evaluating Lua code.
// No UI dependency — pure engine.  Used by both ImGui and Qt
// frontends in the visualization layer.

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward-declare sol types to avoid leaking sol2 into public headers.
namespace sol {
class state;
}

namespace balsa::lua {

class LuaRepl {
  public:
    LuaRepl();
    ~LuaRepl();

    // Non-copyable, movable.
    LuaRepl(const LuaRepl &) = delete;
    LuaRepl &operator=(const LuaRepl &) = delete;
    LuaRepl(LuaRepl &&) noexcept;
    LuaRepl &operator=(LuaRepl &&) noexcept;

    // ── Execution ───────────────────────────────────────────────────

    // Execute a line of Lua code.  Returns true if the execution
    // succeeded, false on error.  Both output and errors are
    // appended to the output buffer.
    bool execute(const std::string &code);

    // ── Output buffer ───────────────────────────────────────────────
    //
    // All print() output, error messages, and echoed input accumulate
    // here.  The UI reads this buffer for display.

    const std::string &output() const { return _output; }
    void clear_output() { _output.clear(); }

    // ── Command history ─────────────────────────────────────────────

    const std::vector<std::string> &history() const { return _history; }

    // ── Post-execute callback ───────────────────────────────────────
    //
    // Called after each successful execute().  The scene wiring layer
    // uses this to trigger MeshData::rediscover_attributes() etc.

    using PostExecuteCallback = std::function<void()>;
    void set_post_execute_callback(PostExecuteCallback cb) {
        _post_execute_cb = std::move(cb);
    }

    // ── Direct state access ─────────────────────────────────────────
    //
    // For binding registration (quiver::lua::load_bindings,
    // balsa::geometry::lua::load_bindings, etc.)

    sol::state &lua_state();

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

    std::string _output;
    std::vector<std::string> _history;
    PostExecuteCallback _post_execute_cb;
};

} // namespace balsa::lua
