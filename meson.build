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

executable('rodlegacy',
    sources: c_sources + cpp_sources + [embedded_resources],
    include_directories: inc_dirs,
    dependencies: [
        sdl2_dep,
        sdl2_image_dep,
        sdl2_mixer_dep,
        adflib_dep,
        lodepng_dep,
        tinyfiledialogs_dep,
        exepack_dep,
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

executable('pc_speaker_test',
    sources: [
        'src/audio/pc_speaker.cpp',
        'src/audio/pc_speaker_test.cpp',
    ],
    include_directories: inc_dirs,
    dependencies: [sdl2_dep, exepack_dep],
    cpp_args: cpp_args,
)


sdl3_dep = dependency('sdl3', required: false)
if sdl3_dep.found()
    executable('amiga_music_player',
        sources: [
            'src/tools/music_player/music_player.cpp',
            'src/parsers/adf.c',
        ],
        include_directories: [inc_dirs],
        dependencies: [sdl3_dep, adflib_dep],
        cpp_args: cpp_args,
        c_args: c_args,
    )
endif

subdir('tests')
