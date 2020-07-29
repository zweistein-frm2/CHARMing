cmake_minimum_required (VERSION 3.16)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(DO_CPPCHECK OFF)
if(NOT DEFINED EMULATOR_ONLY)
set(EMULATOR_ONLY OFF)
endif()
message("use cmake  -DEMULATOR_ONLY=ON   to build mesytec-emulator")

if(${DO_CPPCHECK} STREQUAL ON )
message(cppcheck does not support C++ 17)
if(WIN32)
find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck PATHS "C:\\Program Files\\cppcheck")
else()
find_program(CMAKE_CXX_CPPCHECK NAMES cppcheck)
endif()
if (CMAKE_CXX_CPPCHECK)
    list(
        APPEND CMAKE_CXX_CPPCHECK
            " --suppress='*:magic_enum.hpp'"
            " --suppress='*:boost/*'"
            " --suppress='*:cstdint.hpp'"
            " --inline-suppr"

    )
    message(**CMAKE_CXX_CPPCHECK=${CMAKE_CXX_CPPCHECK})
endif()
endif()

project(CHARMing)
add_subdirectory(libuv)
target_compile_options(uv_a PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fPIC>)
set(libuv_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libuv/include)
set(Charm_DIR ${CMAKE_SOURCE_DIR}/charm)
if(${EMULATOR_ONLY} STREQUAL OFF)
add_subdirectory(charm)
add_subdirectory(entangle-charm-mesytec)
else()
add_subdirectory(mesytec-emulator)
endif()