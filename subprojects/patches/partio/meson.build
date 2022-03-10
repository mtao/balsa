# PARTIO SOFTWARE
# Copyright 2010 Disney Enterprises, Inc. All rights reserved
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in
# the documentation and/or other materials provided with the
# distribution.
# 
# * The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
# Studios" or the names of its contributors may NOT be used to
# endorse or promote products derived from this software without
# specific prior written permission from Walt Disney Pictures.
# 
# Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
# BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
# IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
project('partio', 'cpp', default_options : ['default_library=static'])
partio_version_major = '1'
## Setup platform specific helper defines build variants
cc = meson.get_compiler('cpp')
if target_machine.system() == 'windows'
  if cc.get_id() == 'msvc'
    add_project_arguments('-DPARTIO_WIN32', language: 'cpp')
  endif
  add_project_arguments('-D_USE_MATH_DEFINES', language: 'cpp')
else
  # add_compile_options(['-Wextra', '-Wno-unused-parameter'])
endif
if target_machine.system() == 'linux'
  add_project_link_arguments('-undefined dynamic_lookup', language: 'cpp')
  add_project_arguments('-DGL_SILENCE_DEPRECATION', language: 'cpp')
endif

## Search for useful libraries
Threads_dep = dependency('Threads')
if get_option('opengl')
  GLUT_dep = dependency('GLUT', required: false)
  OpenGL_dep = dependency('OpenGL', required: false)
endif
if get_option('python')
  PythonInterp_dep = dependency('PythonInterp')
  PythonLibs_dep = dependency('PythonLibs')
endif
ZLIB_dep = dependency('ZLIB', required: false)
if ZLIB_dep.found()
  add_project_arguments('-DPARTIO_USE_ZLIB', language: 'cpp')
endif
## Traverse subdirectories
subdir('src/lib')
subdir('src/tools')
#if 'PARTIO_PYTHON_ENABLED'
#  subdir('src/py')
#endif
#subdir('src/tests')
#subdir('src/doc')
