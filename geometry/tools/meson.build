
cli11_dep = dependency('CLI11', required: false)


if get_option('partio') 
  executable('xyz_to_partio', 'xyz_to_partio.cpp', dependencies: [geometry_dep])
endif
if cli11_dep.found()
  executable('mesh_info', 'mesh_info.cpp', dependencies: [geometry_dep, cli11_dep])
endif
