
cli11_dep = dependency('CLI11', required: false, allow_fallback: true)

# ── mesh_viewer_glfw ─────────────────────────────────────────────────
mesh_viewer_glfw_deps = [visualization_dep]
mesh_viewer_glfw_args = []
if cli11_dep.found()
  mesh_viewer_glfw_deps += cli11_dep
  mesh_viewer_glfw_args += '-DBALSA_HAS_CLI11=1'
endif
executable('mesh_viewer_glfw', 'mesh_viewer_glfw.cpp',
  dependencies: mesh_viewer_glfw_deps,
  cpp_args: mesh_viewer_glfw_args)

# ── mesh_viewer_qt ───────────────────────────────────────────────────
mesh_viewer_qt_deps = [visualization_dep]
mesh_viewer_qt_args = []
if cli11_dep.found()
  mesh_viewer_qt_deps += cli11_dep
  mesh_viewer_qt_args += '-DBALSA_HAS_CLI11=1'
endif
executable('mesh_viewer_qt', 'mesh_viewer_qt.cpp',
  dependencies: mesh_viewer_qt_deps,
  cpp_args: mesh_viewer_qt_args)

# ── image_viewer_glfw ────────────────────────────────────────────────
image_viewer_glfw_deps = [visualization_dep]
image_viewer_glfw_args = []
if cli11_dep.found()
  image_viewer_glfw_deps += cli11_dep
  image_viewer_glfw_args += '-DBALSA_HAS_CLI11=1'
endif
executable('image_viewer_glfw', 'image_viewer_glfw.cpp',
  dependencies: image_viewer_glfw_deps,
  cpp_args: image_viewer_glfw_args)
