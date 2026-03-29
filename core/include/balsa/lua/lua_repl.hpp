#if !defined(BALSA_LUA_LUA_REPL_HPP)
#define BALSA_LUA_LUA_REPL_HPP

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
    auto operator=(const LuaRepl &) -> LuaRepl & = delete;
    LuaRepl(LuaRepl &&) noexcept;
    auto operator=(LuaRepl &&) noexcept -> LuaRepl &;

    // ── Execution ───────────────────────────────────────────────────

    // Execute a line of Lua code.  Returns true if the execution
    // succeeded, false on error.  Both output and errors are
    // appended to the output buffer.
    auto execute(const std::string &code) -> bool;

    // ── Output buffer ───────────────────────────────────────────────
    //
    // All print() output, error messages, and echoed input accumulate
    // here.  The UI reads this buffer for display.

    auto output() const -> const std::string & { return _output; }
    auto clear_output() -> void { _output.clear(); }

    // ── Command history ─────────────────────────────────────────────

    auto history() const -> const std::vector<std::string> & { return _history; }

    // ── Post-execute callback ───────────────────────────────────────
    //
    // Called after each successful execute().  The scene wiring layer
    // uses this to trigger MeshData::rediscover_attributes() etc.

    using PostExecuteCallback = std::function<void()>;
    auto set_post_execute_callback(PostExecuteCallback cb) -> void {
        _post_execute_cb = std::move(cb);
    }

    // ── Direct state access ─────────────────────────────────────────
    //
    // For binding registration (quiver::lua::load_bindings,
    // balsa::geometry::lua::load_bindings, etc.)

    auto lua_state() -> sol::state &;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

    std::string _output;
    std::vector<std::string> _history;
    PostExecuteCallback _post_execute_cb;
};

} // namespace balsa::lua

#endif // BALSA_LUA_LUA_REPL_HPP
