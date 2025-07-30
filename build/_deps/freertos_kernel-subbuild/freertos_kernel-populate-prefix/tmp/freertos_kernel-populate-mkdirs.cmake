# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-src")
  file(MAKE_DIRECTORY "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-src")
endif()
file(MAKE_DIRECTORY
  "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-build"
  "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix"
  "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/tmp"
  "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp"
  "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src"
  "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/tleavitt/pico/projects/picoSignals/build/_deps/freertos_kernel-subbuild/freertos_kernel-populate-prefix/src/freertos_kernel-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
