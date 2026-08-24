project('colormap_shaders', 'cpp', version: '0.0')

colormap_shader_dir = meson.current_source_dir() / 'shaders' / 'glsl'
colormap_shaders_dep = declare_dependency(
  include_directories: include_directories('include'),
  variables: {'shader_dir': colormap_shader_dir},
)

meson.override_dependency('colormap_shaders', colormap_shaders_dep)
