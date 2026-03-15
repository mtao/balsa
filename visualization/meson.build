
vulkan_dep = dependency('vulkan', required: false)
#===========================================
# Visualization 
#===========================================

if not get_option('qt')
  error('Qt required for shader storage, please enable to use visualization!')
endif

shaderc_dep = dependency('shaderc')
 subdir('resources')

if get_option('imgui')
  imgui_dep = dependency('imgui')
  imgui_prefix = imgui_dep.get_variable(pkgconfig: 'prefix')
  imgui_bindings_dir = imgui_prefix / 'res' / 'bindings'
  imgui_bindings_inc = include_directories(imgui_bindings_dir, is_system: true)
endif

glm_dep = dependency('glm')

colormap_proj = subproject('colormap_shaders')
colormap_shaders_dep = colormap_proj.get_variable('colormap_shaders_dep') #, include_type:'system')

# Public deps: propagated to consumers via declare_dependency().
visualization_public_deps = [vulkan_dep, glm_dep, shaderc_dep, dl_lib, colormap_shaders_dep]
# Private deps: used only for building the library (not propagated).
visualization_private_deps = []

visualization_sources = [
  'src/vulkan/film.cpp',
  #'src/vulkan/camera.cpp',
  'src/vulkan/scene_base.cpp',
  'src/shaders/abstract_shader.cpp',
  #'src/shaders/flat.cpp',  # removed: empty source file
  'src/vulkan/native_film.cpp',
  'src/vulkan/window.cpp',
]

if get_option('qt')
  visualization_public_deps += qt_dep
  visualization_qt_sources = [
    'src/qt/vulkan/film.cpp',
    'src/qt/vulkan/window.cpp',
    'src/qt/mesh_viewer/main_window.cpp',
    'src/qt/mesh_viewer/widget.cpp',
    ]



  visualization_moc_headers = [
    'include/balsa/visualization/qt/mesh_viewer/main_window.hpp',
    'include/balsa/visualization/qt/mesh_viewer/widget.hpp',
    ]

  visualization_qt_mocced = qt.compile_moc(
    headers: visualization_moc_headers,
    include_directories: include_dir,
    dependencies: qt_dep
    )

  shader_resource_files = ['src/qt/mesh_viewer/shaders/shaders.qrc']

  visualization_qt_rcced = []

  subdir('src/qt/mesh_viewer/shaders/')

  visualization_sources += visualization_qt_sources
  visualization_sources += visualization_qt_mocced
  visualization_sources += visualization_qt_rcced
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
  visualization_sources += [
    'src/vulkan/imgui_integration.cpp',
    # Platform backends (shipped as source in the Conan package)
    imgui_bindings_dir / 'imgui_impl_vulkan.cpp',
  ]
  if get_option('glfw')
    visualization_sources += [
      imgui_bindings_dir / 'imgui_impl_glfw.cpp',
    ]
  endif
endif

visualization_public_deps += core_dep
visualization_public_deps += geometry_dep
include_dir = [include_directories('include')]

# Private includes: only used when building the library, not propagated to consumers.
lib_private_inc = []
if get_option('imgui')
  lib_private_inc += imgui_bindings_inc
endif

visualization_lib = library('balsaVisualization', visualization_sources, dependencies: visualization_public_deps + visualization_private_deps, include_directories: include_dir + lib_private_inc)

# For consumers: propagate public deps (including imgui shared library).
visualization_dep = declare_dependency(link_with: visualization_lib, dependencies: visualization_public_deps, include_directories: include_dir)

subdir('tests')
subdir('examples')
