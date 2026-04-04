
#===========================================
# Geometry
#===========================================

geometry_sources = [
#  'src/mesh/shapes/cube.cpp', 
#  'src/mesh/shapes/tube.cpp', 
#  'src/mesh/halfedge.cpp', 
#  'src/mesh/halfedge_fv_map.cpp', 
#  'src/mesh/read_obj.cpp', 
#  'src/point_cloud/read_xyz.cpp', 
#  'src/mesh/write_obj.cpp', 
#  'src/mesh/boundary_elements.cpp', 
#  'src/mesh/write_ply.cpp', 
#  'src/mesh/triangle/triangle_wrapper.cpp', 
#  'src/mesh/triangle/mesh.cpp', 
#  'src/mesh/ply.cpp', 
#  'src/mesh/polygon_boundary_triangulation.cpp',
  'src/point_cloud/read_xyz.cpp', 
  'src/polygon_mesh/read_obj.cpp', 
  'src/triangle_mesh/read_obj.cpp', 
]

geometry_required_deps = core_required_deps 
#geometry_required_deps += TBB_dep

if get_option('alembic')
  alembic_dep = dependency('Alembic')
  geometry_required_deps += alembic_dep
endif

if get_option('partio') 
  partio_dep = dependency('partio', required: false)
  if not partio_dep.found()
    partio_opts = cmake.subproject_options()
    partio_opts.add_cmake_defines({'PARTIO_TOOLS_ENABLED': false,'PARTIO_PYTHON_ENABLED': false,'PARTIO_DOC_ENABLED': false})

    partio_subproj = cmake.subproject('partio', options: partio_opts)
    partio_dep = partio_subproj.dependency('partio')
  endif

  geometry_required_deps += partio_dep
  geometry_sources += 'src/point_cloud/partio_loader.cpp'
endif

if get_option('openvdb')
  geometry_required_deps += openvdb_dep
endif

geometry_sources += 'src/point_cloud/write_xyz.cpp'
geometry_sources += 'src/point_cloud/extxyz.cpp'

geometry_required_deps += core_dep

if get_option('quiver')
  geometry_required_deps += quiver_dep
endif

if get_option('lua')
  geometry_required_deps += lua_deps
  geometry_sources += 'src/lua/geometry_bindings.cpp'
endif

include_dir = [include_directories('include')]

geometry_lib = library('balsaGeometry', geometry_sources, include_directories: include_dir, dependencies: geometry_required_deps)
geometry_dep = declare_dependency(link_with: geometry_lib, dependencies: geometry_required_deps, include_directories: include_dir)

if get_option('testing')
  subdir('tests')
endif
subdir('tools')
