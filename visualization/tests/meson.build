catch2_with_main_dep = dependency('catch2-with-main', default_options: ['tests=false'])



test_scene_graph = executable('test_scene_graph', 'test_scene_graph.cpp', dependencies: [visualization_dep,catch2_with_main_dep])
test('scene_graph test', test_scene_graph)

test_embedded_shaders = executable(
  'test_embedded_shaders',
  'test_embedded_shaders.cpp',
  dependencies: [visualization_dep, catch2_with_main_dep],
)
test('embedded shader test', test_embedded_shaders)

if get_option('json')
  test_gltf_document = executable(
    'test_gltf_document',
    'test_gltf_document.cpp',
    # Encoders, to compress test meshes and write test images.
    dependencies: [visualization_dep, catch2_with_main_dep, draco_encoder_dep, meshopt_dep, stb_dep],
  )
  test('gltf document test', test_gltf_document)
endif
