
test_example_scene_dep = static_library('vulkan_scene', 'vulkan_scene.cpp', dependencies: [visualization_dep])
executable('vulkan_window', 'vulkan_window.cpp', 'vulkan_scene.cpp', dependencies: [visualization_dep])
#executable('test_vulkan_window', 'test_vulkan_window.cpp', 'example_vulkan_scene.cpp', dependencies: [visualization_dep])
executable('vulkan_window_glfw', 'vulkan_window_glfw.cpp', dependencies: [visualization_dep], link_with: [test_example_scene_dep])


executable('mesh_viewer', 'mesh_viewer.cpp', dependencies: [visualization_dep])
