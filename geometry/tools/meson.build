
cxxopts_dep = dependency('cxxopts')


if get_option('partio') 
  executable('xyz_to_partio', 'xyz_to_partio.cpp', dependencies: [geometry_dep])
endif
executable('mesh_info', 'mesh_info.cpp', dependencies: [geometry_dep, cxxopts_dep])
