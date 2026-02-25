project('streetrod_legacy', ['cpp', 'c'],
  version : '0.1',
  default_options : [
    'warning_level=2',
    'cpp_std=c++17', # needed for fs::path maybe rewrite
    'buildtype=debug',
    'default_library=static', # Build dependencies as static libraries
  ]
)

# Dependencies
sdl2_dep = dependency('sdl2', static: true)

# Platform-specific SDL2 dependencies will be handled in src/meson.build


subdir('src')
subdir('tests')