project('stb', 'c', version: '0.0')

# Header-only; includes are spelled <stb_image.h>, as with the system package.
stb_dep = declare_dependency(include_directories: include_directories('.'))
meson.override_dependency('stb', stb_dep)
