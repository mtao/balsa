#if !defined(BALSA_GEOMETRY_LUA_BINDINGS_HPP)
#define BALSA_GEOMETRY_LUA_BINDINGS_HPP

// Geometry Lua bindings — registers balsa::geometry operations into
// a sol::state_view under the "geometry" table.
//
// Requires quiver Lua bindings to be loaded first (mesh types must
// be registered).

namespace sol {
class state_view;
}

namespace balsa::geometry::lua {

// Register geometry bindings (bounding_box, volumes, read_obj, etc.)
// into the given Lua state.  Creates a "geometry" table.
auto load_bindings(sol::state_view lua) -> void;

} // namespace balsa::geometry::lua

#endif // BALSA_GEOMETRY_LUA_BINDINGS_HPP
