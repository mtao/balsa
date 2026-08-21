project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++26,vc++latest'])

cc = meson.get_compiler('cpp')
dl_lib = cc.find_library('dl', required: false)

# ── Core dependencies (system or WrapDB subproject fallback) ──────────────
# Force spdlog to static when building as subproject: spdlog's wrapdb meson.build
# leaks -DFMT_EXPORT into interface compile_args when built as shared with std::format,
# which breaks fmt's own headers for any target that also depends on fmt directly.
spdlog_dep = dependency('spdlog', version: '>=1.9.2', default_options: ['tests=disabled', 'default_library=static'])
eigen_dep  = dependency('eigen3', version: '>=3.4.0')

zipper_dep = dependency('zipper', default_options: ['testing=false'])

core_required_deps = [spdlog_dep, eigen_dep, zipper_dep]

# ── Optional dependencies (system packages) ────────────────────────────────
if get_option('protobuf')
  protobuf_dep = dependency('protobuf')
  core_required_deps += [protobuf_dep]
endif

if get_option('perfetto')
  perfetto_dep = dependency('perfetto')
  core_required_deps += [perfetto_dep]
endif

cmake = import('cmake')

if get_option('embree')
  embree_dep = dependency('embree3')
endif

if get_option('openvdb')
  # OpenVDB lacks a .pc file and its FindOpenVDB.cmake is hard for Meson to
  # consume (transitive Boost/TBB find-modules).  Since it's a shared library
  # with its transitive deps already linked in, a simple find_library suffices.
  _openvdb_lib = cc.find_library('openvdb')
  # TBB headers are required at compile time (OpenVDB headers pull them in).
  _tbb_dep = dependency('tbb')
  openvdb_dep = declare_dependency(
    dependencies: [_openvdb_lib, _tbb_dep],
  )
endif

if get_option('qt')
  qt = import('qt6')
  qt_required_modules = ['Core']
  if get_option('visualization')
    qt_required_modules += ['Gui', 'Widgets', 'OpenGLWidgets']
  endif
  qt_dep = dependency('qt6', modules: qt_required_modules)
endif

subdir('core')

quiver_dep = dependency('quiver', default_options: ['testing=false', 'tools=false', 'examples=false'])

subdir('geometry')

if get_option('visualization')
  subdir('visualization')
endif
