#===========================================
# Image: pixel storage and decoding
#===========================================

# stb is private: it does not appear in public headers.
stb_dep = dependency('stb')

image_include_dir = [include_directories('include')]
image_lib = library('balsaImage',
  ['src/Image.cpp', 'src/convert.cpp', 'src/decode.cpp'],
  include_directories: image_include_dir,
  dependencies: [stb_dep, zipper_dep])
image_dep = declare_dependency(link_with: image_lib, include_directories: image_include_dir,
  dependencies: [zipper_dep])
meson.override_dependency('balsaImage', image_dep)

if get_option('testing')
  subdir('tests')
endif
