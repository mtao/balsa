
# catch2__dep = dependency('catch2')
catch2_with_main_dep = dependency('catch2-with-main')

test_asset_dir = meson.current_source_dir() / 'assets'

vis_enabled = get_option('visualization')

if vis_enabled
  executable('test_vulkan_window', 'test_vulkan_window.cpp', dependencies: [visualization_dep])
  executable('test_vulkan_window_glfw', 'test_vulkan_window_glfw.cpp', dependencies: [visualization_dep])


  test_scene_graph = executable('test_scene_graph', 'test_scene_graph.cpp', dependencies: [visualization_dep,catch2_with_main_dep])
  test('scene_graph test', test_scene_graph)
endif


test_iterators= executable('test_iterators', 'test_iterators.cpp', dependencies: [core_dep,catch2_with_main_dep])
test('iterator test', test_iterators)

eigen_test_sources = [
  'test_eigen_stl2eigen.cpp',
  'test_eigen_eigen2span.cpp'
]
test_eigen = executable('test_eigen',  eigen_test_sources , dependencies: [core_dep,catch2_with_main_dep])
test('eigen test', test_eigen)

test_mesh_loading = executable('test_mesh_loading',  'test_mesh_loading.cpp', dependencies: [geometry_dep,catch2_with_main_dep], cpp_args: '-DBALSA_TEST_ASSET_PATH="'+test_asset_dir+'"')
test('mesh_loading test', test_mesh_loading)


geometry_test_sources = [
  'test_winding_number.cpp',
  'test_earclipping.cpp',
  'test_circumcenter.cpp',
  'test_volume.cpp',
  'test_smallest_enclosing_sphere.cpp',
]

test_geometry = executable('test_geometry',  geometry_test_sources, dependencies: [geometry_dep,catch2_with_main_dep])
test('geometry test', test_geometry)



