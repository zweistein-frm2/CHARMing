cmake_minimum_required (VERSION 3.11)
#set(CMAKE_VERBOSE_MAKEFILE ON)
if(NOT DEFINED EMULATOR_ONLY)
set(EMULATOR_ONLY OFF)
endif()

message("use cmake  -DEMULATOR_ONLY=ON   to build mesytec-emulator")

if(NOT DEFINED PACK2RPM)
set(PACK2RPM OFF)
endif()

set(boost_currentversion 1.75.0)
set(BOOST_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/boost/out)

file(MAKE_DIRECTORY ${BOOST_PREFIX})

#set(Boost_DEBUG ON)
#set(Boost_DETAILED_FAILURE_MSG ON)
#set(Boost_VERBOSE ON)
set(Boost_NO_SYSTEM_PATHS ON)

# in ${CMAKE_CURRENT_SOURCE_DIR}/boost/jamroot or in ${CMAKE_CURRENT_SOURCE_DIR}/boost/CMakeLists.txt
# the actual boost version is contained and can be extracted to set boost_currentversion automatically


set(BOOST_ROOT ${BOOST_PREFIX})
if(WIN32)
 set(BOOST_INCLUDEDIR ${BOOST_PREFIX}/include/Boost-${boost_currentversion} )
else()
#APPLE or LINUX
set(BOOST_INCLUDEDIR ${BOOST_PREFIX}/include )
endif()
set(BOOST_LIBRARYDIR ${BOOST_PREFIX}/lib)
set(Boost_DIR ${BOOST_PREFIX}/lib/cmake/Boost-${boost_currentversion})
file(MAKE_DIRECTORY ${Boost_DIR})
message(BOOST_PREFIX=${BOOST_PREFIX})



if(WIN32)
set(INSTALL_DIR  $ENV{USERPROFILE} ) # or $ENV{HOMEDRIVE}$ENV{HOMEPATH}
else()
set(INSTALL_DIR /usr/local/bin)
endif()




project(CHARMing)
set(LIBUV_BUILD_TESTS OFF)
set(LIBUV_BUILD_BENCH OFF)
add_subdirectory(libuv)
target_compile_options(uv_a PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fPIC>)
set(libuv_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libuv/include)
set(Charm_DIR ${CMAKE_SOURCE_DIR}/charm)

set(repository ${Charm_DIR})
message(****"repository=" ${repository})
include(${repository}/version.cmake)
message(---------> git Date : ${GIT_DATE})
string(REGEX MATCH  "[0-9]" NGIT_LATEST_TAG "${GIT_LATEST_TAG}")
message(NGIT_LATEST_TAG=${NGIT_LATEST_TAG})
set(CMAKE_PROJECT_VERSION_MAJOR ${NGIT_LATEST_TAG})
set(CMAKE_PROJECT_VERSION_MINOR ${GIT_NUMBER_OF_COMMITS_SINCE})
set(CMAKE_PROJECT_VERSION_PATCH ${GIT_PATCH})


if(${EMULATOR_ONLY} STREQUAL OFF)

add_subdirectory(charm)
add_subdirectory(entangle-install-charming)
add_subdirectory(entangle-install-isegCC2x)
if(UNIX AND NOT APPLE)
if(${PACK2RPM} STREQUAL ON)
set(CPACK_GENERATOR "RPM")
set(CPACK_RPM_PACKAGE_REQUIRES ${INSTALL_DEPS})
else()
set(CPACK_GENERATOR "DEB")
SET (CPACK_DEBIAN_PACKAGE_DEPENDS ${INSTALL_DEPS})
endif()
set(CPACK_PACKAGE_CONTACT "andreas.langhoff@frm2.tum.de" ) # required
set(CPACK_PACKAGE_VENDOR "https://www.frm2.tum.de/startseite/" )
set (CMAKE_PROJECT_HOMEPAGE_URL "https://www.frm2.tum.de/startseite/")
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})


include(CPack)

else()


endif()


else()
#add_subdirectory(mesytec-emulator)
endif()