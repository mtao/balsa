
vulkan_dep = dependency('vulkan', required: false)
#===========================================
# Visualization 
#===========================================

if not get_option('qt')
  error('Qt required for shader storage, please enable to use visualization!')
endif

shaderc_dep = dependency('shaderc')
 subdir('resources')

# ── ImGui ─────────────────────────────────────────────────────────────────
# The WrapDB subproject compiles backend implementations (vulkan, glfw, …)
# directly into the imgui library.  A system / Conan install ships them as
# separate source files that we must compile ourselves.
#
# Everything imgui-related is resolved here; the rest of the file just uses
# imgui_dep, imgui_backend_sources, and imgui_backend_inc.
imgui_backend_sources = []
imgui_backend_inc = []
if get_option('imgui')
  imgui_dep = dependency('imgui')
  if imgui_dep.type_name() != 'internal'
    imgui_prefix = imgui_dep.get_variable(pkgconfig: 'prefix')
    imgui_bd = imgui_prefix / 'res' / 'bindings'
    imgui_backend_inc = [include_directories(imgui_bd, is_system: true)]
    imgui_backend_sources += imgui_bd / 'imgui_impl_vulkan.cpp'
    if get_option('glfw')
      imgui_backend_sources += imgui_bd / 'imgui_impl_glfw.cpp'
    endif
  endif
endif

colormap_proj = subproject('colormap_shaders')
colormap_shaders_dep = colormap_proj.get_variable('colormap_shaders_dep')

# Public deps: propagated to consumers via declare_dependency().
visualization_public_deps = [vulkan_dep, shaderc_dep, dl_lib, colormap_shaders_dep]
# Private deps: used only for building the library (not propagated).
visualization_private_deps = []

visualization_sources = [
  'src/vulkan/film.cpp',
  'src/vulkan/scene_base.cpp',
  'src/vulkan/pipeline.cpp',
  'src/vulkan/vulkan_buffer.cpp',
  'src/vulkan/mesh_buffers.cpp',
  'src/vulkan/mesh_pipeline.cpp',
  'src/vulkan/mesh_scene.cpp',
  'src/vulkan/vulkan_mesh_drawable.cpp',
  'src/shaders/abstract_shader.cpp',
  'src/vulkan/native_film.cpp',
  'src/vulkan/window.cpp',
  'src/scene_graph/Object.cpp',
  'src/scene_graph/Camera.cpp',
  'src/scene_graph/MeshData.cpp',
  'src/scene_graph/BVHData.cpp',
  'src/scene_graph/DrawableGroup.cpp',
]

if get_option('qt')
  visualization_public_deps += qt_dep
  visualization_qt_sources = [
    'src/qt/vulkan/film.cpp',
    'src/qt/vulkan/window.cpp',
    'src/qt/mesh_controls_widget.cpp',
    'src/qt/vertical_tab_widget.cpp',
    'src/qt/SceneGraphModel.cpp',
    'src/qt/SceneGraphWidget.cpp',
    ]

  visualization_moc_headers = [
    'include/balsa/visualization/qt/mesh_controls_widget.hpp',
    'include/balsa/visualization/qt/vertical_tab_widget.hpp',
    'include/balsa/visualization/qt/SceneGraphModel.hpp',
    'include/balsa/visualization/qt/SceneGraphWidget.hpp',
    ]

  visualization_qt_mocced = qt.compile_moc(
    headers: visualization_moc_headers,
    include_directories: include_dir,
    dependencies: qt_dep
    )

  visualization_sources += visualization_qt_sources
  visualization_sources += visualization_qt_mocced
endif
visualization_sources += glsl_resource_sources

if get_option('glfw')
  glfw_dep = dependency('glfw3')
  visualization_public_deps += glfw_dep
  visualization_sources += [
    'src/glfw/vulkan/film.cpp',
    'src/glfw/vulkan/window.cpp',
  ]
endif

if get_option('imgui')
  visualization_public_deps += imgui_dep
  visualization_sources += 'src/vulkan/imgui_integration.cpp'
  visualization_sources += 'src/vulkan/imgui/mesh_controls_panel.cpp'
  visualization_sources += imgui_backend_sources
endif

visualization_public_deps += core_dep
visualization_public_deps += geometry_dep
visualization_public_deps += quiver_dep
include_dir = [include_directories('include')]

visualization_lib = library('balsaVisualization', visualization_sources,
  dependencies: visualization_public_deps + visualization_private_deps,
  include_directories: include_dir + imgui_backend_inc)

# For consumers: propagate public deps (including imgui shared library).
visualization_dep = declare_dependency(link_with: visualization_lib, dependencies: visualization_public_deps, include_directories: include_dir)

subdir('tests')
subdir('examples')
subdir('tools')
