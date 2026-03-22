
cli11_dep = dependency('CLI11', required: false, allow_fallback: true)

particle_converter_deps = [geometry_dep]
particle_converter_args = []

if cli11_dep.found()
  particle_converter_deps += cli11_dep
  particle_converter_args += '-DBALSA_HAS_CLI11=1'
else
  error('CLI11 is required for particle_converter')
endif

if get_option('openvdb')
  particle_converter_args += '-DBALSA_HAS_OPENVDB=1'
endif

if get_option('partio')
  executable('particle_converter', 'particle_converter.cpp',
    dependencies: particle_converter_deps,
    cpp_args: particle_converter_args)
endif
