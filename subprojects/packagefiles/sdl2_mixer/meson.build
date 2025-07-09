project('sdl2_mixer', 'c')

cmake = import('cmake')

sdl2_mixer_proj = cmake.subproject('sdl2_mixer',
    required: true,
    source_dir: '.',
    cmake_options: [
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        '-DBUILD_SHARED_LIBS=OFF',
        '-DSDL2MIXER_SAMPLES=OFF',
        '-DSDL2MIXER_SAMPLES_INSTALL=OFF'
    ]
)

sdl2_mixer_dep = sdl2_mixer_proj.dependency('SDL2_mixer') 