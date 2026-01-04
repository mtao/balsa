catch2_with_main_dep = dependency('catch2-with-main')

test_iterators= executable('test_iterators', 'test_iterators.cpp', dependencies: [core_dep,catch2_with_main_dep])
test('iterator test', test_iterators)
subdir('eigen')
