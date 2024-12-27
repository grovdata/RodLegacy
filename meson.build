project('streetrod_legacy', ['cpp', 'c'],
  version : '0.1',
  default_options : [
    'warning_level=2',
    'cpp_std=c++17', # needed for fs::path maybe rewrite
    'buildtype=debug',
  ]
)

# Dependencies
sdl2_dep = dependency('sdl2')
sdl2_image_dep = dependency('sdl2_image')
sdl2_mixer_dep = dependency('sdl2_mixer')


subdir('src') 