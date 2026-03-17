
test_example_scene_dep = static_library('vulkan_scene', 'vulkan_scene.cpp', dependencies: [visualization_dep])
executable('vulkan_window', 'vulkan_window.cpp', 'vulkan_scene.cpp', dependencies: [visualization_dep])
#executable('test_vulkan_window', 'test_vulkan_window.cpp', 'example_vulkan_scene.cpp', dependencies: [visualization_dep])
executable('vulkan_window_glfw', 'vulkan_window_glfw.cpp', dependencies: [visualization_dep], link_with: [test_example_scene_dep])

cli11_dep = dependency('CLI11', required: false, allow_fallback: true)

executable('mesh_viewer', 'mesh_viewer.cpp', dependencies: [visualization_dep])

mesh_viewer_glfw_deps = [visualization_dep]
mesh_viewer_glfw_args = []
if cli11_dep.found()
  mesh_viewer_glfw_deps += cli11_dep
  mesh_viewer_glfw_args += '-DBALSA_HAS_CLI11=1'
endif
executable('mesh_viewer_glfw', 'mesh_viewer_glfw.cpp',
  dependencies: mesh_viewer_glfw_deps,
  cpp_args: mesh_viewer_glfw_args)

mesh_viewer_qt_deps = [visualization_dep]
mesh_viewer_qt_args = []
if cli11_dep.found()
  mesh_viewer_qt_deps += cli11_dep
  mesh_viewer_qt_args += '-DBALSA_HAS_CLI11=1'
endif
executable('mesh_viewer_qt', 'mesh_viewer_qt.cpp',
  dependencies: mesh_viewer_qt_deps,
  cpp_args: mesh_viewer_qt_args)
