project('balsa', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++26'])




# min versions
fmt_version = '>=8.0.1'
spdlog_version = '>=1.9.2'
eigen_version = '>=3.4.0'
# range_v3_version = ''



cc = meson.get_compiler('cpp')
dl_lib = cc.find_library('dl')




fmt_dep = dependency('fmt', version: fmt_version)

spdlog_dep = dependency('spdlog', version: spdlog_version)

eigen_dep = dependency('eigen3', version: eigen_version)

mdspan_dep = dependency('mdspan')
zipper_proj = subproject('zipper', default_options:{'testing': false})
zipper_dep = zipper_proj.get_variable('zipper_dep')

range_v3_dep = dependency('range-v3')

core_required_deps = [spdlog_dep, eigen_dep,range_v3_dep, fmt_dep, zipper_dep, mdspan_dep]

if get_option('protobuf')
  protobuf_dep = dependency('protobuf')
  core_required_deps += [protobuf_dep]
endif

if get_option('perfetto')
  perfetto_dep = dependency('perfetto')
  core_required_deps += [perfetto_dep]
endif



TBB_dep = dependency('tbb')


cmake = import('cmake')

if get_option('embree')
  embree_dep = dependency('embree3')
endif



# 'src/algebra/pascal_triangle.cpp',



if get_option('qt')
  #qt= import('qt') // TODO: figure out if this is useful
  qt= import('qt6')
  qt_required_modules = ['Core']
  if get_option('visualization')
    qt_required_modules += ['Gui','Widgets', 'OpenGLWidgets']
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



