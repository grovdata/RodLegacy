project('lodepng', ['cpp', 'c'],
    version: '20241215'
)

lodepng_lib = static_library('lodepng',
    'lodepng.c',
    include_directories: include_directories('.'),
    c_args: ['-DLODEPNG_NO_COMPILE_CPP']
)

lodepng_dep = declare_dependency(
    link_with: lodepng_lib,
    include_directories: include_directories('.'),
    compile_args: ['-DLODEPNG_NO_COMPILE_CPP']
) 