project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20'])


spdlog_dep = dependency('spdlog')

eigen_dep = dependency('eigen3')

nlohmann_json_dep = dependency('nlohmann_json')

TBB_dep = dependency('TBB')



colormap_proj = subproject('colormap-shaders')
colormap_shaders_sources = colormap_proj.get_variable('colormap_sources')
colormap_shaders_include_path  = colormap_proj.get_variable('colormap_shaders_include_path')
# 'src/algebra/pascal_triangle.cpp',


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

include_dir = include_directories('include')


core_lib = library('balsaCore', core_sources, include_directories: include_dir, dependencies: [spdlog_dep, eigen_dep, nlohmann_json_dep])

core_dep = declare_dependency(link_with: core_lib, dependencies: [spdlog_dep, eigen_dep, nlohmann_json_dep], include_directories: include_dir)


qt5= import('qt5')
qt5_dep = dependency('qt5', modules: ['Core', 'Gui'])

visualization_sources = [
  'src/visualization/qt/vulkan/windows/triangle_mesh.cpp',
  'src/visualization/qt/vulkan/renderers/triangle_mesh.cpp',

]
visualization_sources += colormap_shaders_sources

visualization_lib = library('balsaVisualization', visualization_sources, dependencies: [core_dep,qt5_dep], include_directories: [include_dir, colormap_shaders_include_path])
visualization_deps = declare_dependency(link_with: visualization_lib, dependencies: [qt5_dep, core_dep])



subdir('tests')
subdir('tools')
