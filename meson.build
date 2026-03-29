project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++26'])

cc = meson.get_compiler('cpp')
dl_lib = cc.find_library('dl')

# ── Core dependencies (system or WrapDB subproject fallback) ──────────────
spdlog_dep = dependency('spdlog', version: '>=1.9.2', default_options: ['tests=disabled'])
eigen_dep  = dependency('eigen3', version: '>=3.4.0')

zipper_proj = subproject('zipper', default_options: {'testing': false})
zipper_dep = zipper_proj.get_variable('zipper_dep')

# ── Optional: Lua (sol2) ─────────────────────────────────────────────
# Fetch sol2 early so core/ can link against it (before subdir('core')).
lua_deps = []
if get_option('lua')
  if not get_option('quiver')
    error('lua=true requires quiver=true')
  endif
  add_project_arguments('-DBALSA_HAS_LUA=1', language: 'cpp')
  sol2_proj = subproject('sol2')
  sol2_dep = sol2_proj.get_variable('sol2_dep')
  lua_deps += sol2_dep
endif

core_required_deps = [spdlog_dep, eigen_dep, zipper_dep]

# ── Optional dependencies (system only -- enable via Conan or system packages) ──
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

if get_option('quiver')
  add_project_arguments('-DBALSA_HAS_QUIVER=1', language: 'cpp')
endif

subdir('core')

if get_option('quiver')
  quiver_proj = subproject('quiver', default_options: ['testing=false', 'tools=false', 'examples=false'])
  quiver_dep = quiver_proj.get_variable('quiver_dep')
endif

subdir('geometry')

if get_option('visualization')
  subdir('visualization')
endif
