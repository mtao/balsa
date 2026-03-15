catch2_with_main_dep = dependency('catch2-with-main', allow_fallback: true)
test_asset_dir = meson.current_source_dir() / 'assets'

test_mesh_loading = executable('test_mesh_loading',  'test_mesh_loading.cpp', dependencies: [geometry_dep,catch2_with_main_dep], cpp_args: '-DBALSA_TEST_ASSET_PATH="'+test_asset_dir+'"')
test('mesh_loading test', test_mesh_loading)

geometry_test_sources = [
  'test_winding_number.cpp',
  'test_earclipping.cpp',
  'test_circumcenter.cpp',
  'test_volume.cpp',
  'test_bounding_box.cpp',
]


test_geometry = executable('test_geometry',  geometry_test_sources, dependencies: [geometry_dep,catch2_with_main_dep])
test('geometry test', test_geometry)

test_welzl = executable('test_welzl_smallest_sphere',
  'test_smallest_enclosing_sphere.cpp',
  dependencies: [geometry_dep, catch2_with_main_dep])
test('welzl smallest sphere test', test_welzl)
