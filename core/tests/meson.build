catch2_with_main_dep = dependency('catch2-with-main', allow_fallback: true)

test_iterators= executable('test_iterators', 'test_iterators.cpp', dependencies: [core_dep,catch2_with_main_dep])
test('iterator test', test_iterators)

test_uri = executable('test_uri', 'test_uri.cpp', dependencies: [core_dep, catch2_with_main_dep])
test('uri test', test_uri)
if eigen_dep.found()
  subdir('eigen')
endif
