
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


if get_option('qt')
  core_sources += 'src/qt/spdlog_logger.cpp'
  core_required_deps += qt_dep
endif



if get_option('json')
  nlohmann_json_dep = dependency('nlohmann_json')
  core_required_deps += nlohmann_json_dep
endif

core_lib = library('balsaCore', core_sources, include_directories: include_dir, dependencies: core_required_deps)

core_dep = declare_dependency(link_with: core_lib, dependencies: core_required_deps, include_directories: include_dir)
subdir('tests')
