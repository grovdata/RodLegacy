# Use CMake subproject for ADFlib
cmake = import('cmake')

adflib_opts = cmake.subproject_options()
adflib_opts.add_cmake_defines({
    'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
    'BUILD_SHARED_LIBS': 'ON',
    'CMAKE_C_FLAGS': '-w'  # Suppress all warnings from ADFlib
})
# CMAKE_C_FLAGS above doesn't reliably reach the Meson-side targets that the
# cmake module reconstructs from ADFlib's CMakeLists.txt, so also override
# Meson's own warning_level for them directly and append -w to be sure.
adflib_opts.set_override_option('warning_level', '0')
adflib_opts.append_compile_args('c', ['-w'])

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
            'warning_level=0',
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
            'warning_level=0',
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
            'warning_level=0',
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
c_sources = files(
    'SFont.c'
)

# C++ sources
cpp_sources = files(
    'main.cpp',
    'mouse.cpp',
    'parsers/adf.c',
    'state.cpp',
    'types.cpp',
    'bitmap.cpp',
    'animated_bitmap.cpp',
    'scene.cpp',
    'car.cpp',
    'car_builder.cpp',
    'car_system.cpp',
    'cardata.cpp',
    'car_set.cpp',
    'carbitmap.cpp',
    'diner.cpp',
    'driving.cpp',
    'racing.cpp',
    'race/track_gen.cpp',
    'race/race_sim.cpp',
    'race/race_end_message.cpp',
    'challenge.cpp',
    'garage/garage.cpp',
    'garage/garage_menus.cpp',
    'garage/garage_modifications.cpp',
    'garage/garage_tire_change.cpp',
    'engine_bay.cpp',
    'scene_layout.cpp',
    'newspaper.cpp',
    'gas_station.cpp',
    'king_of_county.cpp',
    'name_entry.cpp',
    'assets.cpp',
    'parsers/libp.cpp',
    'parsers/carcolor.cpp',
    'roster.cpp',
    'racer_pool.cpp',
    'save_loader.cpp',
    'game_clock.cpp',
    'bet_acceptance.cpp',
    'debug_overlay.cpp',
    'audio/audio_manager.cpp',
    'audio/amiga_music.cpp',
    'audio/engine_sound.cpp',
    'audio/pc_speaker.cpp',
    'ui/button.cpp',
    'ui/checkbox.cpp',
    'ui/bubble.cpp',
    'ui/label.cpp',
    'ui/hud.cpp',
    'dialogs/dialog.cpp',
    'dialogs/list_dialog.cpp',
    'dialogs/colorpicker.cpp',
    'ui/rectangle.cpp',
    'zip_utils.cpp',
    'config_screen.cpp',
)

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
        '../data/font/bee.png',
        '../data/font/lemon.png',
        '../data/res/icon.png',
        '../data/res/paintcan.png',
        '../data/scenes/garage.toml',
        '../data/scenes/diner.toml',
        '../data/scenes/gas_station.toml',
        '../data/scenes/newspaper.toml',
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
    '.',
    'include'
)

# Add compiler flags specific to GCC
cpp_args = ['-ggdb2', '-O0', '-std=c++17', '-D_HAS_STD_BYTE=0', '-D_GLIBCXX_USE_CXX11_ABI=0']
c_args = ['-ggdb2', '-O0']

# miniz (vendored) built as its own library with warnings suppressed, same
# treatment as the other vendored deps (ADFlib, SDL2_image) - not our code to
# fix warnings in.
miniz_lib = static_library(
    'miniz',
    files(
        'miniz.c',
        'miniz_tdef.c',
        'miniz_tinfl.c',
        'miniz_zip.c'
    ),
    include_directories: inc_dirs,
    c_args: c_args,
    override_options: ['warning_level=0'],
)
miniz_dep = declare_dependency(
    link_with: miniz_lib,
    include_directories: inc_dirs,
)

# Add SDL_MAIN_HANDLED only for Windows builds
if host_machine.system() == 'windows'
    cpp_args += ['-DSDL_MAIN_HANDLED']
    c_args += ['-DSDL_MAIN_HANDLED']
endif

# Dear ImGui (vendored), only when -Ddebug_ui=true. Left out entirely otherwise,
# so a release build links no ImGui and debug_ui.cpp is never compiled - the stub
# in debug_ui.h keeps the call sites compiling either way. The subproject builds
# the sdl2 + sdlrenderer2 backends, the pair matching our SDL_Renderer setup.
debug_ui_enabled = get_option('debug_ui')
if debug_ui_enabled
    imgui_proj = subproject('imgui')
    imgui_dep = imgui_proj.get_variable('imgui_dep')
    cpp_sources += files('debug_ui.cpp')
    # Car/wheel-offset inspector scene, debug builds only.
    cpp_sources += files('car_debug.cpp')
    cpp_args += ['-DRODLEGACY_DEBUG_UI']
    game_extra_deps = [imgui_dep]
else
    game_extra_deps = []
endif

exepack_inc = include_directories('include/exepack/include')
exepack_cpp_lib = static_library(
    'exepack_cpp',
    files('include/exepack/src/mz_exe.cpp', 'include/exepack/src/unpack.cpp'),
    include_directories: exepack_inc,
    cpp_args: cpp_args,
)
exepack_dep = declare_dependency(
    link_with: exepack_cpp_lib,
    include_directories: exepack_inc,
)
