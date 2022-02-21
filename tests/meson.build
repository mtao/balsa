
catch2_dep = dependency('catch2', required: false)
if not catch2_dep.found()
  catch2_proj = subproject('catch2')
  catch2_dep = dependency('catch2')
endif

executable('test_vulkan_window', 'test_vulkan_window.cpp', dependencies: [visualization_deps])


test_iterators= executable('test_iterators', 'test_iterators.cpp', dependencies: [core_dep,catch2_dep])

test('iterator test', test_iterators)
test_eigen = executable('test_eigen', 'test_eigen.cpp', 'test_eigen_stl2eigen.cpp', dependencies: [core_dep,catch2_dep])

test('eigen test', test_eigen)
