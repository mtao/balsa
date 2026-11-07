catch2_with_main_dep = dependency('catch2-with-main')

test_image = executable('test_image', 'test_image.cpp', dependencies: [image_dep, stb_dep, catch2_with_main_dep])
test('image test', test_image)
