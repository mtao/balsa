project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])




# min versions
fmt_version = '>=8.0.1'
spdlog_version = '>=1.9.2'
eigen_version = '>=3.4.0'
# range_v3_version = ''




vulkan_dep = dependency('vulkan', required: false)


fmt_dep = dependency('fmt', required: false, version: fmt_version)
if not fmt_dep.found()
  fmt_proj = subproject('fmt')
  fmt_dep = dependency('fmt', version: fmt_version)
endif

spdlog_dep = dependency('spdlog', required: false, version: spdlog_version)
if not spdlog_dep.found()
  spdlog_proj = subproject('spdlog')
  spdlog_dep = dependency('spdlog', version: spdlog_version)
endif

eigen_dep = dependency('eigen3', required: false, version: eigen_version)
if not eigen_dep.found()
  eigen_proj = subproject('eigen')
  eigen_dep = dependency('eigen3',version: eigen_version)
endif

range_v3_dep = dependency('range-v3', required: false)
if not range_v3_dep.found()
  range_v3_proj = subproject('range-v3')
  range_v3_dep = dependency('range-v3')

  #range_v3_dep = range_v3_proj.dependency('range-v3')
endif

if get_option('protobuf')
  protobuf_dep = dependency('protobuf', required: false)
  if not protobuf_dep.found()
    protobuf_proj = subproject('protobuf')
    protobuf_dep = dependency('protobuf')
  endif
endif


if get_option('glfw')
  glfw_dep = dependency('glfw3')
endif

if get_option('imgui')
  imgui_dep = dependency('imgui', required: false)
  if not imgui_dep.found()
    imgui_proj = subproject('imgui')
    imgui_dep = dependency('imgui')
  endif
endif

glm_dep = dependency('glm', required: false)
if not glm_dep.found()
  glm_proj = subproject('glm')
  glm_dep = dependency('glm')
endif


TBB_dep = dependency('TBB')


cmake = import('cmake')

if get_option('embree')
  embree_subproj = cmake.subproject('embree')
endif


colormap_proj = subproject('colormap-shaders')
colormap_shaders_sources = colormap_proj.get_variable('colormap_sources')
colormap_shaders_include_path  = colormap_proj.get_variable('colormap_shaders_include_path')
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


core_deps = [spdlog_dep, eigen_dep,range_v3_dep, fmt_dep]

if get_option('json')
  nlohmann_json_dep = dependency('nlohmann_json', required: false)
  if not nlohmann_json_dep.found()
    nlohmann_json_proj = subproject('nlohmann_json')
    nlohmann_json_dep = dependency('nlohmann_json')
  endif
  core_deps += nlohmann_json_dep
endif

core_lib = library('balsaCore', core_sources, include_directories: include_dir, dependencies: core_deps)

core_dep = declare_dependency(link_with: core_lib, dependencies: core_deps, include_directories: include_dir)


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

geometry_deps = core_deps 
geometry_deps += TBB_dep

if get_option('partio') 
  partio_dep = dependency('partio', required: false)
  if not partio_dep.found()
    partio_subproj = cmake.subproject('partio')
    partio_dep = partio_subproj.dependency('partio')
  endif

  geometry_deps += partio_dep
  geometry_sources += 'src/geometry/point_cloud/partio_loader.cpp'
endif

geometry_lib = library('balsaGeometry', geometry_sources, include_directories: include_dir, dependencies: geometry_deps)
geometry_dep = declare_dependency(link_with: geometry_lib, dependencies: geometry_deps, include_directories: include_dir)


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

shaderc_dep = dependency('shaderc')
# on ubuntu i had to install shaderc myself and I put it in my home directory...
include_dir += include_directories('/home/mtao/.local/include')
qt5= import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])
subdir('resources')


visualization_sources = [
  'src/visualization/vulkan/film.cpp',
  'src/visualization/vulkan/native_film.cpp',
  #'src/visualization/vulkan/camera.cpp',
  'src/visualization/vulkan/scene_base.cpp',

  'src/visualization/glfw/vulkan/film.cpp',

  'src/visualization/qt/vulkan/windows/scene.cpp',
  'src/visualization/qt/vulkan/renderers/scene.cpp',
  'src/visualization/qt/vulkan/film.cpp',
  'src/visualization/shaders/abstract_shader.cpp',
  'src/visualization/shaders/flat.cpp',
  glsl_resource_sources

]
visualization_sources += colormap_shaders_sources

visualization_dep = [core_dep,qt5_dep, vulkan_dep, glm_dep, shaderc_dep]
if get_option('glfw')
  visualization_dep += glfw_dep
endif

visualization_lib = library('balsaVisualization', visualization_sources, dependencies: visualization_dep, include_directories: [include_dir, colormap_shaders_include_path])
visualization_deps = declare_dependency(link_with: visualization_lib, dependencies: visualization_dep, include_directories: [colormap_shaders_include_path, include_dir])



subdir('tests')
subdir('tools')
