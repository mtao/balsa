
# The existing example scene includes an ImGui overlay, so only build it for
# the GLFW + ImGui frontend. Qt-only examples should not acquire ImGui.
if get_option('glfw') and get_option('imgui')
  test_example_scene_dep = static_library(
    'vulkan_scene',
    'vulkan_scene.cpp',
    dependencies: [visualization_dep],
  )
  executable(
    'vulkan_window_glfw',
    'vulkan_window_glfw.cpp',
    dependencies: [visualization_dep],
    link_with: [test_example_scene_dep],
  )
endif
