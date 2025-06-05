
eigen_test_sources = [
  'test_eigen_stl2eigen.cpp',
  'test_eigen_eigen2span.cpp',
  'test_zipper_compat.cpp'
]
test_eigen = executable('test_eigen',  eigen_test_sources , dependencies: [core_dep,catch2_with_main_dep])
test('eigen test', test_eigen)
