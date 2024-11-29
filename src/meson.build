# Use CMake subproject for ADFlib
cmake = import('cmake')

adflib_proj = cmake.subproject('ADFlib',
    required: true,
    cmake_options: [
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
    ]
)
adflib_dep = adflib_proj.dependency('adf')  # or whatever target name is defined in ADFlib's CMakeLists.txt

# C sources (removed ADFlib sources since they're handled by CMake now)
c_sources = [
    'SFont.c',
    'parsers/adf.c',
]

# C++ sources
cpp_sources = [
    'main.cpp',
    'state.cpp',
    'types.cpp',
    'bitmap.cpp',
    'spritesheet.cpp',
    'scene.cpp',
    'car.cpp',
    'part.cpp',
    'cardata.cpp',
    'carbitmap.cpp',
    'diner.cpp',
    'driving.cpp',
    'racing.cpp',
    'garage.cpp',
    'gas_station.cpp',
    'assets.cpp',
    'ui/button.cpp',
    'ui/bubble.cpp',
    'parsers/libp.cpp',
    'parsers/carcolor.cpp',
    'roster.cpp',
    'debug_overlay.cpp',
]

# Include directories
inc_dirs = include_directories(
    'include',
    '../subprojects/ADFlib/src'  # Add ADFlib headers path
)

executable('streetrod_legacy',
    sources: c_sources + cpp_sources,
    include_directories: inc_dirs,
    dependencies: [
        sdl2_dep,
        sdl2_image_dep,
        sdl2_mixer_dep,
        adflib_dep,  # Add ADFlib dependency here
    ],
    cpp_args: ['-ggdb2', '-O0'],
    c_args: ['-ggdb2', '-O0']
) 