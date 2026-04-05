project(
  'exepack_cpp',
  'cpp',
  version: '1.4.0',
  meson_version: '>= 0.56.0',
  default_options: ['cpp_std=c++17', 'warning_level=1'],
)

inc = include_directories('include')

exepack_cpp_lib = static_library(
  'exepack_cpp',
  files('src/mz_exe.cpp', 'src/unpack.cpp'),
  include_directories: inc,
)

exepack_cpp_dep = declare_dependency(
  link_with: exepack_cpp_lib,
  include_directories: inc,
)
