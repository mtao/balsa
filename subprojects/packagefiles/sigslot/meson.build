project('sigslot', 'cpp',
    version: '1.2.3',
    license: 'MIT',
    default_options: ['cpp_std=c++14'])

thread_dep = dependency('threads')

sigslot_dep = declare_dependency(
    include_directories: include_directories('include'),
    dependencies: thread_dep,
)
