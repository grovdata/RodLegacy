project('streetrod_legacy', ['cpp', 'c'],
  version : '0.6.2',
  license : 'GPL-3.0-or-later',
  license_files : 'LICENSE',
  default_options : [
    'warning_level=2',
    'cpp_std=c++17', # needed for fs::path maybe rewrite
    'buildtype=debug',
    'default_library=static',
  ]
)

# Dependencies
sdl2_dep = dependency('sdl2', static: true)

# Platform-specific SDL2 dependencies will be handled in src/meson.build

subdir('src')

if host_machine.system() == 'linux'
    game_link_args = [
        '-lm', '-ldl', '-lpthread', '-lrt',
        '-lasound', '-lpulse-simple', '-lpulse',
        '-lX11', '-lXext', '-lXcursor', '-lXinerama', '-lXi', '-lXfixes', '-lXrandr', '-lXss', '-lXxf86vm',
        '-ldrm', '-lgbm', '-lwayland-egl', '-lwayland-client', '-lwayland-cursor', '-lxkbcommon', '-ldecor-0',
        '-lxcb', '-lXau', '-lXdmcp', '-lpthread',
        '-lXrender'
    ]
elif host_machine.system() == 'windows'
    # adf.c calls GetTempPathA/GetTempFileNameA. sdl2.pc supplies its own explicit
    # -l list, and kernel32 is not in it, so name it here.
    game_link_args = ['-lkernel32']
else
    game_link_args = []
endif

executable('rodlegacy',
    sources: c_sources + cpp_sources + [embedded_resources],
    include_directories: inc_dirs,
    dependencies: [
        sdl2_dep,
        sdl2_image_dep,
        sdl2_mixer_dep,
        adflib_dep,
        lodepng_dep,
        exepack_dep,
        miniz_dep,
        c64_emu_dep,
    ] + game_extra_deps,
    cpp_args: cpp_args,
    c_args: c_args,
    link_args: game_link_args,
    # No console window on Windows. Ignored on other platforms.
    win_subsystem: 'windows',
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

    executable('sr2_dos_player',
        sources: [
            'src/tools/sr2_dos_player/sr2_dos_player.cpp',
            'src/audio/sr2_mus.cpp',
            'src/include/opl3/opl3.c',
        ],
        include_directories: [inc_dirs],
        dependencies: [sdl3_dep],
        cpp_args: cpp_args,
        c_args: c_args,
    )
endif

subdir('tests')
