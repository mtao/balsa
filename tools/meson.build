
cxxopts_dep = dependency('cxxopts', required: false)
if not cxxopts_dep.found()
  cxxopts_proj = subproject('cxxopts')
  cxxopts_dep = dependency('cxxopts')
endif
