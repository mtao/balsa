
catch2_dep = dependency('catch2', required: false)
if not catch2_dep.found()
  catch2_proj = subproject('catch2')
  catch2_dep = dependency('catch2')
endif

test_asset_dir = meson.current_source_dir() / 'assets'


executable('test_vulkan_window', 'test_vulkan_window.cpp', dependencies: [visualization_deps])
executable('test_vulkan_window_glfw', 'test_vulkan_window_glfw.cpp', dependencies: [visualization_deps])


test_iterators= executable('test_iterators', 'test_iterators.cpp', dependencies: [core_dep,catch2_dep])
test('iterator test', test_iterators)


test_eigen = executable('test_eigen', 'main.cpp', 'test_eigen_stl2eigen.cpp', dependencies: [core_dep,catch2_dep])
test('eigen test', test_eigen)

test_mesh_loading = executable('test_mesh_loading', 'main.cpp', 'test_mesh_loading.cpp', dependencies: [geometry_dep,catch2_dep], cpp_args: '-DBALSA_TEST_ASSET_PATH="'+test_asset_dir+'"')
test('mesh_loading test', test_mesh_loading)


geometry_test_sources = [
  'test_winding_number.cpp',
  'test_earclipping.cpp',
]

test_geometry = executable('test_geometry', 'main.cpp', geometry_test_sources, dependencies: [geometry_dep,catch2_dep])
test('geometry test', test_geometry)



test_scene_graph = executable('test_scene_graph', 'test_scene_graph.cpp', dependencies: [visualization_deps,catch2_dep])
test('scene_graph test', test_scene_graph)
