# Thin meson wrapper around draco's CMake build. Provides `draco_dep` so
# consumers can simply call dependency('draco').
project('draco', 'cpp')

cmake = import('cmake')
draco_opts = cmake.subproject_options()
draco_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS' : false,
  'DRACO_TESTS' : false,
  'DRACO_TRANSCODER_SUPPORTED' : false,
})
draco_sub = cmake.subproject('draco-src', options : draco_opts)

# draco's CMake names its library target `draco` under MSVC and
# `draco_static` elsewhere; meson ignores the draco::draco alias.
draco_target = meson.get_compiler('cpp').get_id() == 'msvc' ? 'draco' : 'draco_static'
draco_dep = draco_sub.dependency(draco_target)
meson.override_dependency('draco', draco_dep)
