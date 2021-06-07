set(boost_currentversion 1.75.0)
set(CMAKE_VERBOSE_MAKEFILE ON)
get_filename_component(parentdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY )

if(DEFINED CHARMING_DIR)
  set(BOOST_PREFIX ${CHARMING_DIR}/boost/out)
  set( parentdir ${CHARMING_DIR})
else()
  set(BOOST_PREFIX ${parentdir}/boost/out)
endif()

message(parentdir=${parentdir})

include(${parentdir}/initboost.cmake)
#set(Boost_DEBUG ON)
#set(Boost_DETAILED_FAILURE_MSG ON)
#set(Boost_VERBOSE ON)
set(Boost_NO_SYSTEM_PATHS ON)


set(BOOST_ROOT ${BOOST_PREFIX})
if(WIN32)
 set(BOOST_INCLUDEDIR ${BOOST_PREFIX}/include/Boost-${boost_currentversion} )
else()
 #APPLE or LINUX
 set(BOOST_INCLUDEDIR ${BOOST_PREFIX}/include )
endif()
set(BOOST_LIBRARYDIR ${BOOST_PREFIX}/lib)
set(Boost_DIR ${BOOST_PREFIX}/lib/cmake/Boost-${boost_currentversion})
#file(MAKE_DIRECTORY ${Boost_DIR})
message(STATUS "BOOST_PREFIX=${BOOST_PREFIX}")

#set(Charm_DIR ${parentdir}/charm )
#set(repository ${Charm_DIR})

set(Boost_USE_STATIC_LIBS ON)

if(DEFINED CHARMING_DIR)
# usually only done when cross compiling from a Windows machine to target linux
if(UNIX)

execute_process(COMMAND rm -r sans1_charming
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE var1
                )

message(STATUS ${var1})

execute_process(COMMAND git clone https://github.com/zweistein22/sans1_charming.git
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE var1
                )

message(STATUS ${var1})

endif()
endif()


execute_process(COMMAND git checkout master
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sans1_charming
                ERROR_QUIET
                )

execute_process(COMMAND git pull
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sans1_charming
                ERROR_QUIET
                )

execute_process(COMMAND git submodule init
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sans1_charming
                ERROR_QUIET
                )

execute_process(COMMAND git submodule update
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sans1_charming
                ERROR_QUIET
                )

# CMakeList.txt : CMake project for Boost_Installer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)

execute_process(COMMAND lsb_release -cs
    OUTPUT_VARIABLE RELEASE_CODENAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
   ERROR_QUIET
)

project ("nicos-install-sans1_charming")
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

set(CMAKE_CXX_STANDARD 17)
#message(CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE  RESOURCE_FILES3 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "sans1_charming/*.py"
    "sans1_charming/*.ui"
    "sans1_charming/*.conf"
	"sans1_charming/*.png"
    "sans1_charming/*.sh"
    "sans1_charming/*.md"
)

message(RESOURCE_FILES3=${RESOURCE_FILES3})


include(CMakeRC.cmake)

cmrc_add_resource_library(${PROJECT_NAME}_cmrc ALIAS installer::rc4 NAMESPACE resources ${RESOURCE_FILES3})
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost ${boost_currentversion} REQUIRED COMPONENTS filesystem chrono thread  iostreams ${BOOST_PATHS}) # header only libraries must not be added here
include_directories("${Boost_INCLUDE_DIRS}" )

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)

#include_directories(${Charm_DIR})
# Add source to this project's executable.
add_executable(${PROJECT_NAME} "nicos-install-sans1_charming.cpp" "glibc2.27_compat2.2.5.cpp")

if("${RELEASE_CODENAME}" STREQUAL "bionic")
   message(STATUS 'ubuntu 18 - bionic')
   add_definitions(-DLINUX_FLAVOUR_UBUNTU18)
   target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wl,--wrap=glob>)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)


target_link_libraries(${PROJECT_NAME} PRIVATE installer::rc4)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries( ${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>)
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>)
# TODO: Add tests and install targets if needed.


install(TARGETS ${PROJECT_NAME}
    DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION  "${INSTALL_DIR}"
    )