project('deps', 'c', 'cpp')

# ADFlib (CMake subproject)
cmake = import('cmake')
adflib_proj = cmake.subproject('ADFlib', 
    required: true,
    cmake_options: [
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
    ]
)
adflib_dep = adflib_proj.dependency('adf')

# Load our build configurations
subdir('lodepng.build')
subdir('nfd.build')

# Add size optimization options
if get_option('buildtype') == 'release'
  add_project_arguments('-Os', '-fno-exceptions', '-fno-rtti', '-ffunction-sections', '-fdata-sections', language: 'cpp')
  add_project_link_arguments('-Wl,--gc-sections', language: 'cpp')
endif

# Disable debug logging in release builds
if get_option('buildtype') == 'release'
  add_project_arguments('-DNDEBUG', language: 'cpp')
endif

# Export all dependencies
meson.override_dependency('adflib', adflib_dep)
meson.override_dependency('lodepng', lodepng_dep)
meson.override_dependency('nfd', nfd_dep) 