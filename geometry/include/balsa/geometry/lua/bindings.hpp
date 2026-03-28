#pragma once

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
void load_bindings(sol::state_view lua);

} // namespace balsa::geometry::lua
