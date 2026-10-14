#pragma once
#include <cstddef>
#include <utility>

#include <sigslot/signal.hpp>

namespace balsa::scene_graph {

// ── Signal ──────────────────────────────────────────────────────────
//
// A single-threaded sigslot signal that anyone can connect to but only
// Owner can emit, so listeners can trust that an event really happened.
//
// connect() accepts anything sigslot::signal_st::connect does: callables,
// or a member function pointer plus an object.  Connecting a member
// function of a sigslot::observer_st disconnects automatically when that
// observer is destroyed; connections also end when the Signal is.

template <typename Owner, typename... Args>
class Signal {
  public:
    template <typename... Slot>
    auto connect(Slot &&...slot) -> sigslot::connection {
        return _signal.connect(std::forward<Slot>(slot)...);
    }
    template <typename... Slot>
    auto disconnect(Slot &&...slot) -> std::size_t {
        return _signal.disconnect(std::forward<Slot>(slot)...);
    }
    [[nodiscard]] auto slot_count() -> std::size_t {
        return _signal.slot_count();
    }

  private:
    friend Owner;
    auto emit(Args... args) -> void { _signal(args...); }

    sigslot::signal_st<Args...> _signal;
};

} // namespace balsa::scene_graph
