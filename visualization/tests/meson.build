catch2_with_main_dep = dependency('catch2-with-main', allow_fallback: true)



test_scene_graph = executable('test_scene_graph', 'test_scene_graph.cpp', dependencies: [visualization_dep,catch2_with_main_dep])
test('scene_graph test', test_scene_graph)
