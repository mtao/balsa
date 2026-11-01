catch2_with_main_dep = dependency('catch2-with-main')

test_iterators= executable('test_iterators', 'test_iterators.cpp', dependencies: [core_dep,catch2_with_main_dep])
test('iterator test', test_iterators)

test_uri = executable('test_uri', 'test_uri.cpp', dependencies: [core_dep, catch2_with_main_dep])
test('uri test', test_uri)

test_data_uri = executable('test_data_uri', 'test_data_uri.cpp', dependencies: [core_dep, catch2_with_main_dep])
test('data uri test', test_data_uri)

test_resolver = executable('test_resolver', 'test_resolver.cpp', dependencies: [core_dep, catch2_with_main_dep])
test('resolver test', test_resolver)

test_capitalization = executable('test_capitalization', 'test_capitalization.cpp', dependencies: [core_dep, catch2_with_main_dep])
test('capitalization test', test_capitalization)
if eigen_dep.found()
  subdir('eigen')
endif
