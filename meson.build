project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])




# min versions
fmt_version = '>=8.0.1'
spdlog_version = '>=1.9.2'
eigen_version = '>=3.4.0'
# range_v3_version = ''



cc = meson.get_compiler('cpp')
dl_lib = cc.find_library('dl')


vulkan_dep = dependency('vulkan', required: false)


fmt_dep = dependency('fmt', version: fmt_version)

spdlog_dep = dependency('spdlog', version: spdlog_version)

eigen_dep = dependency('eigen3', version: eigen_version)

zipper_proj = subproject('zipper')
zipper_dep = zipper_proj.get_variable('zipper_dep')

range_v3_dep = dependency('range-v3')

if get_option('protobuf')
  protobuf_dep = dependency('protobuf')
endif




TBB_dep = dependency('tbb')


cmake = import('cmake')

if get_option('embree')
  embree_dep = dependency('embree3')
endif


if get_option('qt')
  #qt= import('qt') // TODO: figure out if this is useful
  qt= import('qt6')
  qt_required_modules = ['Core']
  if get_option('visualization')
    qt_required_modules += ['Gui','Widgets', 'OpenGLWidgets']
  endif
  qt_dep = dependency('qt6', modules: qt_required_modules)
endif

# 'src/algebra/pascal_triangle.cpp',


#===========================================
# Core 
#===========================================

filesystem_sources = [
   'src/filesystem/get_relative_path.cpp', 
   'src/filesystem/prepend_to_filename.cpp'
 ]
filesystem_headers = [
  'include/balsa/filesystem/get_relative_path.hpp',
  'include/balsa/filesystem/prepend_to_filename.hpp',
]

core_sources = [
  'src/logging/stopwatch.cpp',
  'src/logging/json_sink.cpp', 
 ] + filesystem_sources

core_headers = [
  'include/eigen/types.hpp'
] + filesystem_headers

include_dir = [include_directories('include')]

core_required_deps = [spdlog_dep, eigen_dep,range_v3_dep, fmt_dep, zipper_dep]

if get_option('qt')
  core_sources += 'src/qt/spdlog_logger.cpp'
  core_required_deps += qt_dep
endif



if get_option('json')
  nlohmann_json_dep = dependency('nlohmann_json')
  core_required_deps += nlohmann_json_dep
endif



#===========================================
# Geometry
#===========================================

geometry_sources = [
  'src/geometry/mesh/shapes/cube.cpp', 
  'src/geometry/mesh/shapes/tube.cpp', 
  'src/geometry/mesh/halfedge.cpp', 
  'src/geometry/mesh/halfedge_fv_map.cpp', 
  'src/geometry/mesh/read_obj.cpp', 
  'src/geometry/point_cloud/read_xyz.cpp', 
  'src/geometry/mesh/write_obj.cpp', 
  'src/geometry/mesh/boundary_elements.cpp', 
  'src/geometry/mesh/write_ply.cpp', 
  'src/geometry/mesh/triangle/triangle_wrapper.cpp', 
  'src/geometry/mesh/triangle/mesh.cpp', 
  'src/geometry/mesh/ply.cpp', 
  'src/geometry/mesh/polygon_boundary_triangulation.cpp'
]

geometry_sources = [
  'src/geometry/point_cloud/read_xyz.cpp', 
  'src/geometry/polygon_mesh/read_obj.cpp', 
  'src/geometry/triangle_mesh/read_obj.cpp', 
]

geometry_required_deps = core_required_deps 
geometry_required_deps += TBB_dep

if get_option('alembic')
  alembic_dep = dependency('alembic')
  core_required_deps += alembic_dep
endif

if get_option('partio') 
  #partio_dep = dependency('partio', required: false)
  #if not partio_dep.found()
  partio_opts = cmake.subproject_options()
  partio_opts.add_cmake_defines({'PARTIO_TOOLS_ENABLED': false,
'PARTIO_PYTHON_ENABLED': false,
'PARTIO_DOC_ENABLED': false})

  partio_subproj = cmake.subproject('partio', options: partio_opts)
  partio_dep = partio_subproj.dependency('partio')
  #endif

  geometry_required_deps += partio_dep
  geometry_sources += 'src/geometry/point_cloud/partio_loader.cpp'
endif



geometry_headers = [
  'include/mtao/geometry/mesh/halfedge.hpp', 
  'include/mtao/geometry/mesh/halfedge_fv_map.hpp', 
  'include/mtao/geometry/mesh/read_obj.hpp', 
  'include/mtao/geometry/mesh/boundary_matrix.h', 
  'include/mtao/geometry/mesh/boundary_facets.h'
]



#===========================================
# Visualization 
#===========================================

if get_option('visualization')
  if not get_option('qt')
    error('Qt required for shader storage, please enable to use visualization!')
  endif

  shaderc_dep = dependency('shaderc')
  # on ubuntu i had to install shaderc myself and I put it in my home directory...
  include_dir += include_directories('/home/mtao/.local/include')
  subdir('resources')

  if get_option('imgui')
    imgui_dep = dependency('imgui')
  endif

  glm_dep = dependency('glm')

  colormap_proj = subproject('colormap_shaders')
  colormap_shaders_dep = colormap_proj.get_variable('colormap_shaders_dep') #, include_type:'system')
  
  visualization_sources = [
    'src/visualization/vulkan/film.cpp',
    #'src/visualization/vulkan/camera.cpp',
    'src/visualization/vulkan/scene_base.cpp',
    'src/visualization/shaders/abstract_shader.cpp',
    'src/visualization/shaders/flat.cpp',
    'src/visualization/vulkan/native_film.cpp',
    'src/visualization/vulkan/window.cpp',
  ]

  visualization_qt_sources = [
    'src/visualization/qt/vulkan/windows/scene.cpp',
    'src/visualization/qt/vulkan/renderers/scene.cpp',
    'src/visualization/qt/vulkan/film.cpp',
    #'src/visualization/qt/vulkan/window.cpp',
    'src/visualization/qt/mesh_viewer/main_window.cpp',
    'src/visualization/qt/mesh_viewer/widget.cpp',
    ]

  visualization_moc_headers = [
    'include/balsa/visualization/qt/mesh_viewer/main_window.hpp',
    ]

  visualization_qt_mocced = qt.compile_moc(
    headers: visualization_moc_headers,
    include_directories: include_dir,
    dependencies: qt_dep
    )


  visualization_sources += visualization_qt_sources
  visualization_sources += visualization_qt_mocced
  visualization_sources += glsl_resource_sources
  
  visualization_required_deps = [qt_dep, vulkan_dep, glm_dep, shaderc_dep, dl_lib, colormap_shaders_dep]


  


  if get_option('glfw')
    glfw_dep = dependency('glfw3')
    visualization_required_deps += glfw_dep
    visualization_sources += [
      'src/visualization/glfw/window.cpp',
      'src/visualization/glfw/vulkan/film.cpp',
      'src/visualization/glfw/vulkan/window.cpp',
    ]
  endif

endif

core_lib = library('balsaCore', core_sources, include_directories: include_dir, dependencies: core_required_deps)

core_dep = declare_dependency(link_with: core_lib, dependencies: core_required_deps, include_directories: include_dir)


geometry_required_deps += core_dep

geometry_lib = library('balsaGeometry', geometry_sources, include_directories: include_dir, dependencies: geometry_required_deps)
geometry_dep = declare_dependency(link_with: geometry_lib, dependencies: geometry_required_deps, include_directories: include_dir)



if get_option('visualization')
  visualization_required_deps += core_dep
  visualization_required_deps += geometry_dep

  visualization_lib = library('balsaVisualization', visualization_sources, dependencies: visualization_required_deps, include_directories: [include_dir])
  visualization_dep = declare_dependency(link_with: visualization_lib, dependencies: visualization_required_deps, include_directories: [include_dir])
  
endif



subdir('tests')
subdir('examples')
subdir('tools')
