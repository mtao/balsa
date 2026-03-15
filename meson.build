project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++26'])

cc = meson.get_compiler('cpp')
dl_lib = cc.find_library('dl')

# ── Core dependencies (system or WrapDB subproject fallback) ──────────────
fmt_dep    = dependency('fmt',    version: '>=8.0.1')
spdlog_dep = dependency('spdlog', version: '>=1.9.2')
eigen_dep  = dependency('eigen3', version: '>=3.4.0')

zipper_proj = subproject('zipper', default_options: {'testing': false})
zipper_dep = zipper_proj.get_variable('zipper_dep')

range_v3_dep = dependency('range-v3')

core_required_deps = [spdlog_dep, eigen_dep, range_v3_dep, fmt_dep, zipper_dep]

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

if get_option('qt')
  qt = import('qt6')
  qt_required_modules = ['Core']
  if get_option('visualization')
    qt_required_modules += ['Gui', 'Widgets', 'OpenGLWidgets']
  endif
  qt_dep = dependency('qt6', modules: qt_required_modules)
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
