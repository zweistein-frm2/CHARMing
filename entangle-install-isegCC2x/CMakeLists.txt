﻿# CMakeList.txt : CMake project for Boost_Installer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)


project ("entangle-install-isegCC2x")
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

set(CMAKE_CXX_STANDARD 17)
message(CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE  RESOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.py"
    "*.res"
    "*.pdf"
)

message(RESOURCE_FILES=${RESOURCE_FILES})


include(CMakeRC.cmake)

cmrc_add_resource_library(installer-isegCC2x ALIAS installer::rc NAMESPACE resources ${RESOURCE_FILES})
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread  iostreams ${BOOST_PATHS}) # header only libraries must not be added here
include_directories("${Boost_INCLUDE_DIRS}" )
include_directories("../entangle-install-charming")
set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)

include_directories(${Charm_DIR})
# Add source to this project's executable.
add_executable (${PROJECT_NAME} "entangle-Install-isegCC2x.cpp" )
target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>)
target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)

target_link_libraries(${PROJECT_NAME} PRIVATE installer::rc)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries( ${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:libstdc++.a>)
# TODO: Add tests and install targets if needed.


install(TARGETS ${PROJECT_NAME}
    DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION  "${INSTALL_DIR}"
    )