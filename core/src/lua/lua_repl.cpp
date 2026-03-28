#include "balsa/lua/lua_repl.hpp"

#include <spdlog/spdlog.h>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace balsa::lua {

// ── Pimpl ───────────────────────────────────────────────────────────

struct LuaRepl::Impl {
    sol::state lua;
};

// ── Construction / destruction ──────────────────────────────────────

LuaRepl::LuaRepl() : _impl(std::make_unique<Impl>()) {
    // Open standard Lua libraries.
    _impl->lua.open_libraries(sol::lib::base,
                              sol::lib::string,
                              sol::lib::table,
                              sol::lib::math,
                              sol::lib::io,
                              sol::lib::os,
                              sol::lib::package);

    // Redirect print() to our output buffer.
    _impl->lua.set_function("print", [this](sol::variadic_args va) {
        bool first = true;
        for (auto arg : va) {
            if (!first) _output += '\t';
            first = false;

            // Use Lua's tostring() for consistent formatting.
            sol::object obj = arg;
            auto ts = _impl->lua["tostring"];
            if (ts.valid()) {
                sol::protected_function_result r = ts(obj);
                if (r.valid()) {
                    std::string s = r.get<std::string>();
                    _output += s;
                } else {
                    _output += "<tostring error>";
                }
            } else {
                _output += "<no tostring>";
            }
        }
        _output += '\n';
    });
}

LuaRepl::~LuaRepl() = default;

LuaRepl::LuaRepl(LuaRepl &&) noexcept = default;
LuaRepl &LuaRepl::operator=(LuaRepl &&) noexcept = default;

// ── Execution ───────────────────────────────────────────────────────

bool LuaRepl::execute(const std::string &code) {
    if (code.empty()) return true;

    // Record in history.
    _history.push_back(code);

    // Echo the input.
    _output += "> " + code + '\n';

    // Try as expression first (return value), then as statement.
    // This mimics standard REPL behavior: "1+2" prints 3.
    std::string expr_code = "return " + code;
    auto result = _impl->lua.safe_script(expr_code, sol::script_pass_on_error);

    if (!result.valid()) {
        // Not a valid expression — try as a statement.
        result = _impl->lua.safe_script(code, sol::script_pass_on_error);
    }

    if (!result.valid()) {
        sol::error err = result;
        _output += std::string("[error] ") + err.what() + '\n';
        return false;
    }

    // If the expression returned a value, print it.
    if (result.return_count() > 0) {
        sol::object val = result;
        if (val.valid() && val.get_type() != sol::type::none
            && val.get_type() != sol::type::lua_nil) {
            auto ts = _impl->lua["tostring"];
            if (ts.valid()) {
                sol::protected_function_result r = ts(val);
                if (r.valid()) { _output += r.get<std::string>() + '\n'; }
            }
        }
    }

    // Invoke post-execute callback.
    if (_post_execute_cb) { _post_execute_cb(); }

    return true;
}

// ── Direct state access ─────────────────────────────────────────────

sol::state &LuaRepl::lua_state() { return _impl->lua; }

} // namespace balsa::lua
