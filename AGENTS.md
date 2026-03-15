# AGENTS.md -- Balsa Codebase Context

## Project Overview

**Balsa** is a C++26 computational geometry and visualization library by [mtao](https://github.com/mtao). It is a rewrite/cleanup of [mtao/core](https://github.com/mtao/core) with:
- A `balsa::` namespace (replacing the old `mtao::` namespace -- migration is ongoing)
- Focus on Qt6 and Vulkan for visualization
- Meson build system (replacing CMake)
- Heavy use of C++20/23/26 features: concepts, `if constexpr`, fold expressions, `std::mdspan`, ranges

The name "balsa" refers to the lightweight base material for building things.

## Repository Structure

```
balsa/
  core/                  # Core library: tensor types, Eigen/zipper wrappers, logging, caching, filesystem utils
  geometry/              # Geometry library: mesh I/O, simplex ops, bounding boxes, point cloud algorithms
  visualization/         # Visualization library: Vulkan renderer, Qt/GLFW windowing, scene graph, shaders
  subprojects/           # Meson wrap dependencies (zipper, colormap_shaders, partio, mdspan)
  tests/                 # Top-level test directory (empty -- tests live inside each module)
  meson.build            # Root build file
  meson_options.txt      # Build options (15 boolean toggles)
  conanfile.py           # Conan 2.x dependency management
  setup.sh               # Build setup script: conan install + meson setup + ninja
  compile_flags.txt      # clangd config: -std=c++20 -I./include
  .clang-format          # Code style: 4-space indent, no column limit, right-aligned pointers
  .projections.json      # Vim projectionist: src/*.cpp <-> include/balsa/*.hpp
```

## Build System

### Toolchain
- **Build system**: Meson (with Ninja backend)
- **Package manager**: Conan 2.x (generates PkgConfig files consumed by Meson)
- **C++ standard**: C++26 (`cpp_std=c++26` in meson.build, though `compile_flags.txt` says c++20 for tooling)
- **Test framework**: Catch2

### How to Build
```bash
# Option 1: Use the setup script
./setup.sh [conan-profile-name]

# Option 2: Manual
conan install . --output-folder=build/conan --build=missing
cd build
meson setup --native-file conan/conan_meson_native.ini .. .
ninja
```

### Build Options (`meson_options.txt`)
All are boolean toggles. Key ones:
| Option | Default | Description |
|--------|---------|-------------|
| `visualization` | true | Build the visualization library |
| `qt` | true | Qt6 integration |
| `protobuf` | true | Protobuf support |
| `json` | true | nlohmann_json |
| `partio` | true | Particle I/O library |
| `igl` | true | libigl integration |
| `embree` | false | Intel Embree ray tracing |
| `openvdb` | false | OpenVDB sparse volumes |
| `perfetto` | false | Performance tracing |

### Core Dependencies
| Library | Version | Purpose |
|---------|---------|---------|
| Eigen 3.4 | Linear algebra (legacy backend) |
| spdlog 1.14 | Logging |
| fmt 11.0 | String formatting |
| range-v3 | Ranges library (used in parsing and data transforms) |
| mdspan 0.6 | C++23 mdspan reference implementation |
| zipper (git subproject) | mdspan-based tensor library (new backend, by same author) |
| Catch2 3.7 | Testing |
| cxxopts 3.2 | CLI argument parsing |

### Visualization Dependencies (when enabled)
Vulkan (headers/loader), shaderc, GLFW 3.3, GLM, ImGui 1.88, Qt 6.8

## Architecture

### Three Main Libraries

```
meson dependency graph:
  balsaCore       -> core_dep
  balsaGeometry   -> geometry_dep  (depends on core_dep)
  balsaVisualization -> vis_dep    (depends on core_dep + geometry_dep)
```

### 1. `core/` -- `balsaCore` Library

**Headers**: `core/include/balsa/`
**Sources**: `core/src/`
**Tests**: `core/tests/`

Key sub-modules:

- **`tensor_types.hpp`** -- The main entry point for numeric types. Aliases zipper tensor types into the `balsa` namespace: `Matrix`, `Vector`, `ColVectors`, `RowVectors`, plus concrete names like `Vec3d`, `Mat4f`, `ColVecs3d`. Uses `std::dynamic_extent` for dynamic sizing.

- **`eigen/`** -- Extensive Eigen utilities:
  - `types.hpp` -- Eigen-native type aliases (mirrors tensor_types but with `Eigen::Dynamic`)
  - `stl2eigen.hpp` -- Convert `std::vector`, `std::array`, nested containers to Eigen types (zero-copy via `Eigen::Map` where possible)
  - `eigen2span.hpp` -- Convert Eigen matrices to `std::span`
  - `stack.hpp` -- Variadic `hstack()`/`vstack()` for Eigen matrices
  - `interweave.hpp` -- Interleave rows/columns from multiple matrices
  - `shape_checks.hpp` -- Runtime/compile-time dimension validation
  - `slice.hpp` -- Generic matrix slicing by index containers
  - `zipper_compat.hpp` -- **Critical bridge**: bidirectional conversion between Eigen and zipper types (`as_zipper()`, `as_eigen()`)
  - `concepts/` -- 30+ C++20 concepts for constraining Eigen types by base class and shape (`MatrixBaseDerived`, `Vec3Compatible`, `ColVecs2Compatible`, `IntegralMatrix`, etc.)

- **`zipper/`** -- Zipper tensor library types and concepts:
  - `types.hpp` -- Type aliases using `::zipper::Matrix`, `::zipper::Vector`
  - `VectorContainer.hpp` -- Owning container with CRTP base
  - `stl2zipper.hpp` -- STL-to-zipper conversion (goes through Eigen bridge)
  - `concepts/` -- Shape concepts mirroring `eigen/concepts/` but for zipper types

- **`concepts/tensor_shapes.hpp`** -- Backend-agnostic shape concepts (the "canonical" set for library consumers)

- **`logging/`** -- `HierarchicalStopwatch` (RAII scoped timer with JSON output), JSON spdlog sink
- **`caching/`** -- `SerializableCache` (LRU with serialization), `Prefetcher` (async sequential prefetch)
- **`filesystem/`** -- `get_relative_path()`, `prepend_to_filename()`
- **`qt/`** -- Redirects Qt's qDebug/qWarning to spdlog
- **`types/`** -- Metaprogramming: `has_tuple_size` concept, `is_specialization_of` trait, type name demangling
- **`data_structures/`** -- `StackedContiguousBuffer` (flat buffer with offset spans, like CSR row pointers)

### 2. `geometry/` -- `balsaGeometry` Library

**Headers**: `geometry/include/balsa/geometry/`
**Sources**: `geometry/src/`
**Tests**: `geometry/tests/`
**Tools**: `geometry/tools/` (mesh_info CLI, xyz_to_partio converter)

Key sub-modules:

- **Root headers**:
  - `BoundingBox.hpp` -- `BoundingBox<T, Dim>` class (AABB, zipper-based vectors)
  - `bounding_box.hpp` -- Free function `bounding_box(V)` for Eigen and zipper matrices
  - `winding_number.hpp` -- 2D winding number for point-in-polygon tests
  - `get_angle.hpp` -- 2D angle utilities

- **`simplex/`** -- Single-simplex operations:
  - `volume.hpp` -- Signed/unsigned volume via determinant
  - `circumcenter.hpp` -- Circumcenter via SPD (Cholesky) or SPSD (QR) solvers
  - `point_in_circumcircle.hpp` -- Circumcircle containment test

- **`simplicial_complex/`** -- Operations over meshes (collections of simplices):
  - `volumes.hpp`, `circumcenters.hpp` -- Batch per-simplex computations
  - `boundaries.hpp` -- Extract boundary facets

- **`triangle_mesh/`**:
  - `TriangleMesh.hpp` -- Simple struct: `vertices` + `triangles` + `edges`
  - `read_obj.hpp` -- OBJ reader (reads as polygon, then triangulates)
  - `earclipping.hpp` -- Ear-clipping polygon triangulation

- **`polygon_mesh/`**:
  - `PolygonBuffer.hpp` -- Variable-length polygon storage (backed by `StackedContiguousBuffer`)
  - `PLCurveBuffer.hpp` -- Piecewise-linear curve storage with open/closed flag
  - `PolygonMesh.hpp` -- Struct: `vertices` + `polygons` + `curves`
  - `read_obj.hpp` -- Full OBJ parser (vertices, textures, normals, faces, curves)
  - `triangulate_polygons.hpp` -- Fan triangulation for quads, ear-clipping for larger polygons

- **`point_cloud/`**:
  - `smallest_enclosing_sphere_welzl.hpp` -- Welzl's randomized algorithm
  - `bridson_poisson_disk_sampling.hpp` -- Blue-noise sampling (legacy `mtao::` namespace)
  - `read_xyz.hpp` -- XYZ file reader
  - `partio_loader.hpp` -- Partio particle I/O (optional)

- **`acceleration/`** -- `AlternatingDigitalTree` (kd-tree stub, not yet implemented)

### 3. `visualization/` -- `balsaVisualization` Library

**Headers**: `visualization/include/balsa/`
**Sources**: `visualization/src/`
**Tests**: `visualization/tests/`
**Examples**: `visualization/examples/` (mesh_viewer, vulkan_window, vulkan_window_glfw)

Key sub-modules:

- **`scene_graph/`** -- Generic, rendering-API-agnostic scene graph (inspired by [Magnum](https://magnum.graphics)):
  - `embedding_traits.hpp` -- Traits struct defining geometric space (dimension, scalar, glm types)
  - `object.hpp` -- Scene node with parent/children tree, transform mixin, feature container
  - `abstract_feature.hpp` -- Attachable component on objects
  - `features/bounding_box.hpp` -- Cached AABB feature
  - `camera.hpp` -- Camera object (empty shell, inherits transform)
  - All classes are templates parameterized by transformation/embedding types

- **`vulkan/`** -- Core Vulkan rendering:
  - `film.hpp` -- `Film` pure virtual interface abstracting the rendering surface (swapchain, command buffers, render pass, depth/stencil, MSAA)
  - `native_film.hpp` -- `NativeFilm` full Vulkan implementation (instance, device selection, swapchain lifecycle, synchronization) using `vk::raii::*`
  - `scene_base.hpp` -- Base for drawable scenes
  - `window.hpp` -- Abstract Vulkan window with scene management and draw loop
  - `drawable.hpp` -- Feature that can be drawn with a camera
  - `pipeline.hpp` -- Vulkan pipeline wrapper

- **`shaders/`** -- Shader system:
  - `abstract_shader.hpp` -- Runtime GLSL-to-SPIR-V compilation via shaderc, loads from Qt resources (`:/glsl/...`)
  - `shader.hpp` -- Template adding dimension preprocessor macros (`TWO_DIMENSIONS`/`THREE_DIMENSIONS`)
  - `flat.hpp` -- Flat (unlit) shader implementation
  - GLSL resources in `resources/glsl/` (flat.vert/frag, triangle.vert/frag)

- **`glfw/`** -- GLFW windowing backend:
  - `window.hpp` -- GLFW window with full callback handling
  - `vulkan/film.hpp` -- NativeFilm + GLFW surface creation
  - `vulkan/window.hpp` -- Combined GLFW + Vulkan window

- **`qt/`** -- Qt windowing backend:
  - `vulkan/film.hpp` -- Film wrapping `QVulkanWindow`
  - `vulkan/window.hpp` -- Template `QVulkanWindow` with renderer creation
  - `mesh_viewer/` -- Complete Qt OpenGL mesh viewer application (MainWindow, Widget with VBO/IBO, Phong shaders, mouse interaction)

## Key Patterns and Conventions

### Dual Tensor Backends
The codebase is transitioning from **Eigen** to **zipper** (an mdspan-based tensor library by the same author). Both are supported:
- `balsa::eigen::*` -- Eigen-native types and utilities
- `balsa::zipper::*` -- Zipper-native types
- `balsa::*` (top-level) -- Aliases to zipper types (the new default)
- `balsa::eigen::as_zipper()` / `balsa::eigen::as_eigen()` -- Bridge functions

When writing new code, prefer zipper types (`balsa::Vec3d`, `balsa::ColVecs3d`). Use `as_eigen()` only when interfacing with Eigen-specific APIs.

### C++20 Concepts Everywhere
Template parameters are constrained using concepts organized in layers:
1. **Base type concepts**: `MatrixBaseDerived`, `ZipperBaseDerived` -- is it an Eigen/zipper type?
2. **Shape concepts**: `Vec3Compatible`, `ColVecs2Compatible`, `SquareMatrix3Compatible` -- compile-time dimension checks
3. **Scalar concepts**: `IntegralMatrix`, `IntegralVecCompatible` -- scalar type checks

Example: `template <eigen::concepts::ColVecs3Compatible T> auto volume(const T& V)` -- only accepts 3-row column-vector matrices.

### Column-Vector Convention
Points/vertices are stored as **column vectors** in matrices. A set of N 3D points is a `ColVectors<double, 3>` (3 rows, N columns). This matches Eigen's convention and makes OpenGL striding natural.

### Header Layout Convention
- Public headers: `<module>/include/balsa/<module_path>/*.hpp`
- Source files: `<module>/src/<subpath>/*.cpp`
- Vim projectionist maps: `src/*.cpp` <-> `include/balsa/*.hpp`

### Namespace Migration (Legacy Code)
Some files still use `mtao::` or `mtao::geometry::` namespace. These are legacy and should eventually be migrated to `balsa::geometry::`. Known legacy files:
- `geometry/include/balsa/geometry/point_cloud/bridson_poisson_disk_sampling.hpp`
- `geometry/include/balsa/geometry/point_cloud/vdb_particle_list.hpp`
- `geometry/include/balsa/geometry/curves/winding_number.hpp`
- `core/include/balsa/eigen/slice_filter.hpp`

### Film Abstraction (Visualization)
The `Film` class is the central Vulkan surface abstraction. It decouples rendering logic from windowing:
- `NativeFilm` -- full standalone Vulkan (used by GLFW backend)
- `qt::vulkan::Film` -- wraps Qt's `QVulkanWindow`
- Scenes/drawables receive a `Film&` reference, never directly touch windowing

## Testing

Tests live inside each module (`core/tests/`, `geometry/tests/`, `visualization/tests/`), not in the top-level `tests/` dir. They use Catch2 and are registered with Meson's test runner.

### Running Tests
```bash
cd build
meson test -v
```

### Test Coverage
- `core/tests/`: iterator tests, Eigen/zipper conversion tests
- `geometry/tests/`: bounding box, circumcenter, ear clipping, mesh loading, smallest enclosing sphere, volume, winding number
- `visualization/tests/`: scene graph (object hierarchy, bounding box features)

## CI

- **`.github/workflows/conan.yaml`** -- Validates Conan dependency resolution (triggers on `conanfile.py` changes)
- **`.github/workflows/continuous.yml`** -- Full build + test pipeline (currently **disabled** -- `on:` block is commented out)

## Subprojects

| Project | Source | Notes |
|---------|--------|-------|
| zipper | `github.com/mtao/zipper` (main branch) | mdspan-based tensor library, core dependency |
| colormap_shaders | `github.com/mtao/colormap-shaders` | GLSL colormaps, used in visualization |
| partio | `github.com/mtao/partio` | Particle I/O, optional |
| mdspan | Redirects to `zipper/subprojects/mdspan.wrap` | C++23 mdspan reference impl |

## Common Tasks

### Adding a New Geometric Algorithm
1. Create header in `geometry/include/balsa/geometry/<subdir>/`
2. If it needs compiled sources, add `.cpp` to `geometry/src/<subdir>/` and register in `geometry/meson.build`
3. Use `balsa::` tensor types and constrain with concepts from `balsa/concepts/tensor_shapes.hpp`
4. Add Catch2 tests in `geometry/tests/`

### Adding a New Core Utility
1. Create header in `core/include/balsa/<subdir>/`
2. Register sources in `core/meson.build` if needed
3. Add to `core_required_deps` if it needs new dependencies

### Working with Meshes
```cpp
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
auto mesh = balsa::geometry::triangle_mesh::read_objD("model.obj");
// mesh.position.vertices  -- ColVectors<double, 3>
// mesh.position.triangles -- ColVectors<index_type, 3>
```

### Converting Between Tensor Backends
```cpp
#include <balsa/eigen/zipper_compat.hpp>
Eigen::MatrixXd eigen_mat = ...;
auto zipper_mat = balsa::eigen::as_zipper(eigen_mat);  // lazy view
auto eigen_back = balsa::eigen::as_eigen(zipper_mat);  // lazy NullaryExpr
```
