# Use CMake subproject for ADFlib
cmake = import('cmake')

adflib_proj = cmake.subproject('ADFlib', 
    required: true,
    cmake_options: [
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
    ]
)

adflib_dep = adflib_proj.dependency('adf')

# Get lodepng dependency
lodepng_proj = subproject('lodepng')
lodepng_dep = lodepng_proj.get_variable('lodepng_dep')

# Load nativefiledialog-extended
nfd_proj = subproject('nativefiledialog-extended')
nfd_dep = nfd_proj.get_variable('nfde_dep')

# C sources
c_sources = [
    'SFont.c'
]

# C++ sources
cpp_sources = [
    'main.cpp',
    'parsers/adf.c',
    'state.cpp',
    'types.cpp',
    'bitmap.cpp',
    'spritesheet.cpp',
    'scene.cpp',
    'car.cpp',
    'cardata.cpp',
    'carbitmap.cpp',
    'diner.cpp',
    'driving.cpp',
    'racing.cpp',
    'garage.cpp',
    'newspaper.cpp',
    'gas_station.cpp',
    'assets.cpp',
    'parsers/libp.cpp',
    'parsers/carcolor.cpp',
    'roster.cpp',
    'debug_overlay.cpp',
    'ui/button.cpp',
    'ui/bubble.cpp',
    'ui/label.cpp',
    'ui/hud.cpp',
    'dialogs/dialog.cpp',    
    'dialogs/list_dialog.cpp',
    'dialogs/colorpicker.cpp',
    'ui/rectangle.cpp',
]

# Include directories
inc_dirs = include_directories(
    'include',
    '../subprojects/nativefiledialog-extended-1.2.1/src/include'
)

# Add compiler flags specific to GCC
cpp_args = ['-ggdb2', '-O0']
c_args = ['-ggdb2', '-O0']


executable('open_sr',
    sources: c_sources + cpp_sources,
    include_directories: inc_dirs,
    dependencies: [
        sdl2_dep,
        sdl2_image_dep,
        sdl2_mixer_dep,
        adflib_dep,
        lodepng_dep,
        nfd_dep,
    ],
    cpp_args: cpp_args,
    c_args: c_args
)