
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
endif

glm_dep = dependency('glm')

colormap_proj = subproject('colormap_shaders')
colormap_shaders_dep = colormap_proj.get_variable('colormap_shaders_dep') #, include_type:'system')

visualization_required_deps = [vulkan_dep, glm_dep, shaderc_dep, dl_lib, colormap_shaders_dep]

visualization_sources = [
  'src/vulkan/film.cpp',
  #'src/vulkan/camera.cpp',
  'src/vulkan/scene_base.cpp',
  'src/shaders/abstract_shader.cpp',
  'src/shaders/flat.cpp',
  'src/vulkan/native_film.cpp',
  'src/vulkan/window.cpp',
]

if get_option('qt')
  visualization_required_deps += qt_dep
  visualization_qt_sources = [
    'src/qt/vulkan/windows/scene.cpp',
    'src/qt/vulkan/renderers/scene.cpp',
    'src/qt/vulkan/film.cpp',
    #'src/qt/vulkan/window.cpp',
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
  visualization_required_deps += glfw_dep
  visualization_sources += [
    'src/glfw/window.cpp',
    'src/glfw/vulkan/film.cpp',
    'src/glfw/vulkan/window.cpp',
  ]
endif

visualization_required_deps += core_dep
visualization_required_deps += geometry_dep
include_dir = [include_directories('include')]

visualization_lib = library('balsaVisualization', visualization_sources, dependencies: visualization_required_deps, include_directories: [include_dir])
visualization_dep = declare_dependency(link_with: visualization_lib, dependencies: visualization_required_deps, include_directories: [include_dir])

subdir('tests')
subdir('examples')
