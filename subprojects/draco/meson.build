# Thin meson wrapper around draco's CMake build. Provides `draco_dep` (the
# decoder) so consumers can call dependency('draco'), and
# `draco_encoder_dep` (decoder and encoder) for tests.
project('draco', 'cpp')

cmake = import('cmake')
draco_opts = cmake.subproject_options()
draco_opts.add_cmake_defines({
  'BUILD_SHARED_LIBS' : false,
  'DRACO_TESTS' : false,
  'DRACO_JS_GLUE' : false,
  'DRACO_TRANSCODER_SUPPORTED' : false,
})
draco_opts.set_install(false)
draco_sub = cmake.subproject('draco-src', options : draco_opts)

# draco_static has no sources of its own (it bundles object libraries), so
# meson's CMake bridge builds it without PIC and it cannot be linked into
# shared libraries.  The object libraries themselves are PIC, so depend on
# those instead.
draco_object_deps = []
foreach target : ['attributes',
                  'compression_attributes_dec',
                  'compression_attributes_pred_schemes_dec',
                  'compression_bit_coders',
                  'compression_decode',
                  'compression_entropy',
                  'compression_mesh_dec',
                  'compression_mesh_traverser',
                  'compression_options',
                  'compression_point_cloud_dec',
                  'core',
                  'dec_config',
                  'mesh',
                  'metadata',
                  'metadata_dec',
                  'point_cloud',
                  'points_dec']
  draco_object_deps += draco_sub.dependency('draco_' + target, include_type : 'system')
endforeach
draco_dep = declare_dependency(dependencies : draco_object_deps)
meson.override_dependency('draco', draco_dep)

draco_encoder_object_deps = []
foreach target : ['compression_attributes_enc',
                  'compression_attributes_pred_schemes_enc',
                  'compression_encode',
                  'compression_mesh_enc',
                  'compression_point_cloud_enc',
                  'enc_config',
                  'metadata_enc',
                  'points_enc']
  draco_encoder_object_deps += draco_sub.dependency('draco_' + target, include_type : 'system')
endforeach
draco_encoder_dep = declare_dependency(dependencies : [draco_dep] + draco_encoder_object_deps)
