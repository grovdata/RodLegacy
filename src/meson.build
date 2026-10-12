# Use CMake subproject for ADFlib
cmake = import('cmake')

adflib_opts = cmake.subproject_options()
adflib_opts.add_cmake_defines({
    'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
    'BUILD_SHARED_LIBS': 'OFF',
    'CMAKE_C_FLAGS': '-w',  # Suppress all warnings from ADFlib
    'ADFLIB_ENABLE_TESTS': 'OFF'  # its test suite FetchContent-clones libcheck from GitHub
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
    'gfx/SFont.c'
)

# C++ sources
cpp_sources = files(
    'main.cpp',
    'debug/main_debug.cpp',
    'core/mouse.cpp',
    'core/gamepad.cpp',
    'core/recorder.cpp',
    'ui/control_icons.cpp',
    'dialogs/key_bindings_dialog.cpp',
    'dialogs/settings_dialog.cpp',
    'parsers/adf.c',
    # Nuked-OPL3, for Street Rod 2's AdLib engine sound.
    'include' / 'opl3' / 'opl3.c',
    'core/state.cpp',
    'core/types.cpp',
    'core/video.cpp',
    'gfx/bitmap.cpp',
    'gfx/animated_bitmap.cpp',
    'scenes/scene.cpp',
    'car/car.cpp',
    'car/car_builder.cpp',
    'car/car_system.cpp',
    'car/cardata.cpp',
    'car/car_set.cpp',
    'car/carbitmap.cpp',
    'scenes/diner.cpp',
    'race/driving.cpp',
    'race/racing.cpp',
    'race/racing_render.cpp',
    'race/racing_debug.cpp',
    'race/track_gen.cpp',
    'race/aqueduct.cpp',
    'race/grudge.cpp',
    'scenes/grudge_night.cpp',
    'race/race_sim.cpp',
    'race/sr2_gauge.cpp',
    'race/race_end_message.cpp',
    'race/challenge.cpp',
    'garage/garage.cpp',
    'garage/garage_menus.cpp',
    'garage/garage_modifications.cpp',
    'garage/garage_tire_change.cpp',
    'garage/garage_transmission_install.cpp',
    'garage/garage_engine_bay_install.cpp',
    'garage/garage_sr2_engine_bay.cpp',
    'garage/garage_sell_car.cpp',
    'garage/garage_under_car.cpp',
    'car/car_haggle.cpp',
    'car/engine_bay.cpp',
    'car/engine_bay_layout.cpp',
    'car/sr2_engine_bay_layout.cpp',
    'scenes/scene_layout.cpp',
    'scenes/newspaper.cpp',
    'scenes/gas_station.cpp',
    'scenes/gas_station_pump.cpp',
    'race/king_of_county.cpp',
    'scenes/name_entry.cpp',
    'scenes/save_load_scene.cpp',
    'scenes/game_over_menu.cpp',
    'scenes/ending.cpp',
    'scenes/intro.cpp',
    'scenes/intro_player.cpp',
    'scenes/sr2_end_script.cpp',
    'scenes/sr2_ending.cpp',
    'core/assets.cpp',
    'parsers/libp.cpp',
    'parsers/carcolor.cpp',
    'race/roster.cpp',
    'race/racer_pool.cpp',
    'core/save_loader.cpp',
    'core/save_file.cpp',
    'core/game_clock.cpp',
    'core/game_text.cpp',
    'race/bet_acceptance.cpp',
    'debug/debug_overlay.cpp',
    'audio/audio_manager.cpp',
    'audio/amiga_music.cpp',
    'audio/adlib_engine.cpp',
    'audio/adlib_sfx.cpp',
    'audio/pc_speaker_fx.cpp',
    'audio/pc_speaker_sfx.cpp',
    'audio/adlib_music.cpp',
    'audio/sr2_mus.cpp',
    'audio/engine_sound.cpp',
    'audio/pc_speaker.cpp',
    'ui/button.cpp',
    'ui/checkbox.cpp',
    'ui/bubble.cpp',
    'ui/label.cpp',
    'ui/hud.cpp',
    'ui/main_font.cpp',
    'dialogs/dialog.cpp',
    'dialogs/list_dialog.cpp',
    'dialogs/number_entry_dialog.cpp',
    'ui/rectangle.cpp',
    'core/zip_utils.cpp',
    'scenes/config_screen.cpp',
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

    # app.rc carries the exe's VERSIONINFO, so it is generated from the
    # project version. VERSIONINFO wants four integers; pad short versions.
    vp = meson.project_version().split('.')
    app_rc = configure_file(
        input: 'app.rc.in',
        output: 'app.rc',
        configuration: {
            'VERSION_COMMA': '@0@,@1@,@2@,0'.format(
                vp[0],
                vp.length() > 1 ? vp[1] : '0',
                vp.length() > 2 ? vp[2] : '0'),
            'VERSION_STR': meson.project_version(),
        },
    )

    # Compile resource file
    app_res = custom_target('app.res',
        input: app_rc,
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
        '../data/font/g1-7x8-sfont.png',
        '../data/res/icon.png',
        '../data/res/checkbox-on.png',
        '../data/res/checkbox-off.png',
        '../data/res/icons-keyboard-16x16-1bit-ansdor.png',
        '../data/res/icons-controller-16x16-1bit-ansdor.png',
        '../data/scenes/garage.toml',
        '../data/scenes/garage_sr2.toml',
        '../data/scenes/diner.toml',
        '../data/scenes/diner_sr2.toml',
        '../data/scenes/gas_station.toml',
        '../data/scenes/gas_station_sr2.toml',
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
    'include',
    'include' / 'miniz'
)

# Add compiler flags specific to GCC
cpp_args = ['-ggdb2', '-O0', '-std=c++17', '-D_HAS_STD_BYTE=0', '-D_GLIBCXX_USE_CXX11_ABI=0',
            '-DRODLEGACY_VERSION="' + meson.project_version() + '"']
c_args = ['-ggdb2', '-O0']

# miniz (vendored) built as its own library with warnings suppressed, same
# treatment as the other vendored deps (ADFlib, SDL2_image) - not our code to
# fix warnings in.
miniz_lib = static_library(
    'miniz',
    files(
        'include' / 'miniz' / 'miniz.c',
        'include' / 'miniz' / 'miniz_tdef.c',
        'include' / 'miniz' / 'miniz_tinfl.c',
        'include' / 'miniz' / 'miniz_zip.c'
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

# Dear ImGui (vendored), only in debug builds (or -Ddebug_ui=enabled). Left out entirely otherwise,
# so a release build links no ImGui and debug_ui.cpp is never compiled - the stub
# in debug_ui.h keeps the call sites compiling either way. The subproject builds
# the sdl2 + sdlrenderer2 backends, the pair matching our SDL_Renderer setup.
if get_option('debug')
    cpp_args += ['-DRODLEGACY_DEBUG']
endif
debug_ui_enabled = get_option('debug_ui').enabled() or (get_option('debug_ui').auto() and get_option('debug'))
if debug_ui_enabled
    imgui_proj = subproject('imgui-docking', default_options: ['sdl2=enabled', 'sdl2_renderer=enabled'])
    imgui_dep = imgui_proj.get_variable('imgui_dep')
    cpp_sources += files('debug/debug_ui.cpp')
    # Car/wheel-offset inspector scene, debug builds only.
    cpp_sources += files('debug/car_debug.cpp')
    cpp_sources += files('debug/sfx_debug.cpp')
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
