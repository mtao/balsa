
eigen_test_sources = [
  'test_zipper_compat.cpp'
]
test_eigen = executable('test_eigen',  eigen_test_sources , dependencies: [core_dep,eigen_dep,catch2_with_main_dep])
test('eigen test', test_eigen)
