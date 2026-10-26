
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
  'src/types/get_type_name.cpp',
 ] + filesystem_sources

core_headers = filesystem_headers

include_dir = [include_directories('include')]


if get_option('qt')
  core_sources += 'src/qt/spdlog_logger.cpp'
  core_required_deps += qt_dep
endif



if get_option('json')
  # balsa's JSON (de)serialization is driven by C++26 static reflection with
  # annotations (balsa/json/reflection.hpp). As of 2026 only GCC 16+ supports
  # it, and only with -freflection. Disable with -Djson=false otherwise.
  reflection_args = ['-freflection']
  reflection_probe = '''
#include <version>
#if !defined(__cpp_impl_reflection) || !defined(__cpp_lib_reflection)
#error "no C++26 reflection"
#endif
'''
  if not cc.compiles(reflection_probe, args: reflection_args,
                     name: 'C++26 reflection (__cpp_impl_reflection, __cpp_lib_reflection)')
    error('-Djson=true requires C++26 reflection (__cpp_impl_reflection / ' +
          '__cpp_lib_reflection), e.g. GCC 16+ with -freflection; ' +
          'disable it with -Djson=false')
  endif
  nlohmann_json_dep = dependency('nlohmann_json')
  # Propagated: headers using balsa::json annotations need -freflection too.
  core_required_deps += [nlohmann_json_dep,
                         declare_dependency(compile_args: reflection_args)]
endif

# balsa::parallel dispatches to std::execution::par when TBB is available.
tbb_dep = dependency('tbb', required: get_option('parallel'))
core_compile_args = []
if tbb_dep.found()
  core_required_deps += tbb_dep
  core_compile_args += '-DBALSA_PARALLEL'
endif

core_lib = library('balsaCore', core_sources, include_directories: include_dir, dependencies: core_required_deps)

core_dep = declare_dependency(link_with: core_lib, dependencies: core_required_deps, include_directories: include_dir, compile_args: core_compile_args)
meson.override_dependency('balsaCore', core_dep)
if get_option('testing')
  subdir('tests')
endif
