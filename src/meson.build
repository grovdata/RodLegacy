# Use CMake subproject for ADFlib
cmake = import('cmake')

adflib_opts = cmake.subproject_options()
adflib_opts.add_cmake_defines({
    'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
    'BUILD_SHARED_LIBS': 'ON',
    'CMAKE_C_FLAGS': '-w'  # Suppress all warnings from ADFlib
})

adflib_proj = cmake.subproject('ADFlib', 
    required: true,
    options: adflib_opts
)

adflib_dep = adflib_proj.dependency('adf')

# Get lodepng dependency
lodepng_proj = subproject('lodepng',
    default_options: ['default_library=static']
)
lodepng_dep = lodepng_proj.get_variable('lodepng_dep')

# Build tinyfiledialogs locally
tinyfiledialogs_sources = [
    'include/tinyfiledialogs.c'
]

tinyfiledialogs_lib = static_library('tinyfiledialogs',
    tinyfiledialogs_sources,
    c_args: ['-DTINYFD_NOLIB'],
    pic: true
)

tinyfiledialogs_dep = declare_dependency(
    link_with: tinyfiledialogs_lib,
    include_directories: include_directories('include')
)

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
            'use_imageio=disabled',
            'test=false'
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
    # macOS: Use subprojects for static linking (without JPEG XL to avoid brotli)
    sdl2_image_proj = subproject('sdl2_image',
        default_options: [
            'default_library=static',
            'avif=disabled',
            'jxl=disabled',
            'tif=disabled',
            'webp=disabled',
            'svg=disabled',
            'use_imageio=disabled',
            'test=false'
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
elif host_machine.system() == 'windows'
    # Windows: Use subprojects for static linking (for cross-compilation)
    sdl2_image_proj = subproject('sdl2_image',
        default_options: [
            'default_library=static',
            'avif=disabled',
            'jxl=disabled',
            'tif=disabled',
            'webp=disabled',
            'svg=disabled',
            'use_imageio=disabled',
            'test=false'
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
    'mouse.cpp',
    'parsers/adf.c',
    'state.cpp',
    'types.cpp',
    'bitmap.cpp',
    'animated_bitmap.cpp',
    'spritesheet.cpp',
    'scene.cpp',
    'car.cpp',
    'car_builder.cpp',
    'car_system.cpp',
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
    'audio/audio_manager.cpp',
    'audio/engine_sound.cpp',
    'ui/button.cpp',
    'ui/checkbox.cpp',
    'ui/bubble.cpp',
    'ui/label.cpp',
    'ui/hud.cpp',
    'dialogs/dialog.cpp',    
    'dialogs/list_dialog.cpp',
    'dialogs/colorpicker.cpp',
    'ui/rectangle.cpp',
]

# Windows resource compilation
if host_machine.system() == 'windows'
    # Find ImageMagick for icon conversion
    convert = find_program('convert', required: true)
    
    # Find windres for resource compilation
    windres = find_program('i686-w64-mingw32-windres', required: true)
    
    # Convert PNG to ICO using ImageMagick
    icon_ico = custom_target('icon.ico',
        input: '../data/res/icon.png',
        output: 'icon.ico',
        command: [convert, '@INPUT@', '-define', 'icon:auto-resize=16,32,48,64,128,256', '@OUTPUT@'],
        build_by_default: true
    )
    
    # Compile resource file
    app_res = custom_target('app.res',
        input: 'app.rc',
        output: 'app.res',
        command: [windres, '-O', 'coff', '@INPUT@', '-o', '@OUTPUT@'],
        depends: icon_ico,
        build_by_default: true
    )
    
    # Add resource file to sources
    cpp_sources += app_res
endif

# Generate embedded resources (fonts + icon)
python3 = find_program('python3')
embedded_resources = custom_target('embedded_resources',
    input: [
        '../data/font/font.png',
        '../data/font/bee.png',
        '../data/font/lemon.png',
        '../data/res/icon.png',
        '../data/res/paintcan.png'
    ],
    output: ['embedded_resources.h', 'embedded_resources.c'],
    command: [
        python3,
        '@SOURCE_ROOT@/scripts/embed_resources.py',
        '@INPUT@',
        '@OUTDIR@'
    ],
    build_by_default: true
)

# Include directories
inc_dirs = include_directories(
    'include'
)

# Add compiler flags specific to GCC
cpp_args = ['-ggdb2', '-O0', '-std=c++17', '-D_HAS_STD_BYTE=0', '-D_GLIBCXX_USE_CXX11_ABI=0']
c_args = ['-ggdb2', '-O0']

# Add SDL_MAIN_HANDLED only for Windows builds
if host_machine.system() == 'windows'
    cpp_args += ['-DSDL_MAIN_HANDLED']
    c_args += ['-DSDL_MAIN_HANDLED']
endif


executable('open_sr',
    sources: c_sources + cpp_sources + [embedded_resources],
    include_directories: inc_dirs,
    dependencies: [
        sdl2_dep,
        sdl2_image_dep,
        sdl2_mixer_dep,
        adflib_dep,
        lodepng_dep,
        tinyfiledialogs_dep,
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