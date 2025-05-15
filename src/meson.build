# Use CMake subproject for ADFlib
cmake = import('cmake')

adflib_proj = cmake.subproject('ADFlib', 
    required: true,
    cmake_options: [
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DBUILD_SHARED_LIBS=OFF', # Build as static library
    ]
)

adflib_dep = adflib_proj.dependency('adf')

# Get lodepng dependency
lodepng_proj = subproject('lodepng',
    default_options: ['default_library=static']
)
lodepng_dep = lodepng_proj.get_variable('lodepng_dep')

# Load nativefiledialog-extended
nfd_proj = subproject('nativefiledialog-extended',
    default_options: ['default_library=static']
)
nfd_dep = nfd_proj.get_variable('nfde_dep')

# Platform-specific SDL2 dependencies
if host_machine.system() == 'linux'
    # Linux: Use subprojects for static linking
    sdl2_image_proj = subproject('sdl2_image',
        default_options: [
            'default_library=static',
            'avif=disabled',
            'jxl=disabled',
            'tif=disabled',
            'webp=disabled',
            'svg=disabled',
            'use_imageio=disabled'
        ]
    )
    sdl2_image_dep = sdl2_image_proj.get_variable('sdl2_image_dep')

    sdl2_mixer_proj = subproject('sdl2_mixer',
        default_options: [
            'default_library=static',
            'flac=disabled',
            'mpg123=disabled',
            'opus=disabled'
        ]
    )
    sdl2_mixer_dep = sdl2_mixer_proj.get_variable('sdl2_mixer_dep')
elif host_machine.system() == 'darwin'
    # macOS: Use system libraries (easier to manage on macOS)
    sdl2_image_dep = dependency('sdl2_image', static: true)
    sdl2_mixer_dep = dependency('sdl2_mixer', static: true)
elif host_machine.system() == 'windows'
    # Windows: Use system libraries (easier to manage on Windows)
    sdl2_image_dep = dependency('sdl2_image', static: true)
    sdl2_mixer_dep = dependency('sdl2_mixer', static: true)
else
    error('Unsupported platform: ' + host_machine.system())
endif

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
    'animated_bitmap.cpp',
    'spritesheet.cpp',
    'scene.cpp',
    'car.cpp',
    'cardata.cpp',
    'carbitmap.cpp',
    'diner.cpp',
    'driving.cpp',
    'racing.cpp',
    'garage.cpp',
    'engine_bay.cpp',
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
    c_args: c_args,
    link_args: host_machine.system() == 'linux' ? [
        '-lm', '-ldl', '-lpthread', '-lrt',
        '-lasound', '-lpulse-simple', '-lpulse',
        '-lX11', '-lXext', '-lXcursor', '-lXinerama', '-lXi', '-lXfixes', '-lXrandr', '-lXss', '-lXxf86vm',
        '-ldrm', '-lgbm', '-lwayland-egl', '-lwayland-client', '-lwayland-cursor', '-lxkbcommon', '-ldecor-0',
        '-lxcb', '-lXau', '-lXdmcp', '-lpthread',
        '-lXrender'
    ] : []
)