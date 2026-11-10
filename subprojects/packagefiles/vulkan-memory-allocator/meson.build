project('VulkanMemoryAllocator', 'cpp', version: '3.3.0')

vma_dep = declare_dependency(include_directories: include_directories('include', is_system: true))
meson.override_dependency('VulkanMemoryAllocator', vma_dep)
