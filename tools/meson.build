
cxxopts_dep = dependency('cxxopts', required: false)
if not cxxopts_dep.found()
  cxxopts_proj = subproject('cxxopts')
  cxxopts_dep = dependency('cxxopts')
endif


executable('xyz_to_partio', 'xyz_to_partio.cpp', dependencies: [geometry_dep])
executable('mesh_info', 'mesh_info.cpp', dependencies: [geometry_dep, cxxopts_dep])
