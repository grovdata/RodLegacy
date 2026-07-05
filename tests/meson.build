# Unit tests — SDL-free

# Re-declare lodepng (idempotent in Meson) since libp.h includes lodepng.h
lodepng_test_proj = subproject('lodepng',
    default_options: ['default_library=static']
)
lodepng_test_dep = lodepng_test_proj.get_variable('lodepng_dep')

test_inc = include_directories(
    'include',          # doctest/doctest.h
    '../src',           # parsers/libp.h, parsers/carcolor.h
    '../src/include',   # tinyfiledialogs etc (not needed, but harmless)
)

test_sources = [
    'test_main.cpp',
    'test_vectorreader.cpp',
    'test_bitmapparser.cpp',
    'test_queries.cpp',
    'test_track.cpp',
    'test_strings.cpp',
    'test_car_system.cpp',
    'test_engine_part.cpp',
    'test_racer_pool.cpp',
    'test_bet_acceptance.cpp',
    'test_track_gen.cpp',
    'test_race_sim.cpp',
    '../src/race/track_gen.cpp',
    '../src/race/race_sim.cpp',
    '../src/parsers/libp.cpp',
    '../src/car_system.cpp',
    '../src/racer_pool.cpp',
    '../src/bet_acceptance.cpp',
    # carcolor.cpp intentionally excluded: libp.cpp never calls GetStandardCarColor
    # and carcolor.cpp unnecessarily includes <SDL.h>
    # types.cpp intentionally excluded: operator<(CarModel) is not called by any test
]

test_exe = executable('test_libp',
    sources: test_sources,
    include_directories: test_inc,
    dependencies: [lodepng_test_dep],
    cpp_args: ['-std=c++17', '-O0'],
)

test('libp', test_exe)
