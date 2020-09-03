cmake_minimum_required (VERSION 3.16)
set(CMAKE_VERBOSE_MAKEFILE ON)
if(NOT DEFINED EMULATOR_ONLY)
set(EMULATOR_ONLY OFF)
endif()
message("use cmake  -DEMULATOR_ONLY=ON   to build mesytec-emulator")

if(WIN32)
  set(Boost_NO_SYSTEM_PATHS OFF)
 if (${Boost_NO_SYSTEM_PATHS})
  message("HARD_CODED_BOOST_PATH")
  set(BOOST_ROOT 'C:/boost/boost_1_73_0')
  set(BOOST_INCLUDE_DIRS "${BOOST_ROOT}/include")
  set(BOOST_LIBRARY_DIRS "${BOOST_ROOT}/stage/lib")
  set(Boost_DIR "${BOOST_LIBRARY_DIRS}/cmake/Boost-1.73.0" )
  set(BOOST_PATHS "PATHS ${BOOST_ROOT}")
  list(APPEND CMAKE_PREFIX_PATH ${Boost_DIR})
 else()
  message("ON_WINDOWS_BOOST_SHOULD_BE_INSTALLED_OUTSIDE_VCPKG")
   set(BOOST_PATHS )
endif()
#set(Python_ROOT_DIR "C:/Users/alanghof/AppData/Local/Programs/Python/Python37" )
endif()
if(WIN32)
set(INSTALL_DIR  $ENV{USERPROFILE} ) # or $ENV{HOMEDRIVE}$ENV{HOMEPATH}
else()
set(INSTALL_DIR /usr/local/bin)
endif()




project(CHARMing)
add_subdirectory(libuv)
target_compile_options(uv_a PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fPIC>)
set(libuv_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libuv/include)
set(Charm_DIR ${CMAKE_SOURCE_DIR}/charm)

set(repository ${Charm_DIR})
message(****"repository=" ${repository})
include(${repository}/version.cmake)
message(---------> git Date : ${GIT_DATE})
set(CMAKE_PROJECT_VERSION_MAJOR ${GIT_LATEST_TAG})
set(CMAKE_PROJECT_VERSION_MINOR ${GIT_NUMBER_OF_COMMITS_SINCE})
set(CMAKE_PROJECT_VERSION_PATCH ${GIT_PATCH})


if(${EMULATOR_ONLY} STREQUAL OFF)
add_subdirectory(charm)
add_subdirectory(entangle-charming)
add_subdirectory(charm-entangle-install)



if(UNIX AND NOT APPLE)
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andreas Langhoff") # required
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})

SET (CPACK_DEBIAN_PACKAGE_DEPENDS "libopencv-dev")
include(CPack)

else()


endif()


else()
#add_subdirectory(mesytec-emulator)
endif()