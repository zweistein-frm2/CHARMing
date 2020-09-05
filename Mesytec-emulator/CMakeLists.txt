# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)




get_filename_component(parentdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY )

if(DEFINED CHARMING_DIR)
  set(BOOST_PREFIX ${CHARMING_DIR}/boost/out)
  set( parentdir ${CHARMING_DIR})
 else()
  set(BOOST_PREFIX ${parentdir}/boost/out)
endif()

message(parentdir=${parentdir})

#set(Boost_DEBUG ON)
#set(Boost_DETAILED_FAILURE_MSG ON)
#set(Boost_VERBOSE ON)
set(Boost_NO_SYSTEM_PATHS ON)

# in ${CMAKE_CURRENT_SOURCE_DIR}/boost/jamroot or in ${CMAKE_CURRENT_SOURCE_DIR}/boost/CMakeLists.txt
# the actual boost version is contained and can be extracted to set boost_currentversion automatically
set(boost_currentversion 1.75.0)
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
message(BOOST_PREFIX=${BOOST_PREFIX})

set(Charm_DIR ${parentdir}/charm )
set(repository ${Charm_DIR})

set(Boost_USE_STATIC_LIBS ON)

project(mesytec-emulator)
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread  iostreams locale  ${BOOST_PATHS}) # header only libraries must not be added here

set(libuv_INCLUDE_DIR "${parentdir}/libuv/include")
include_directories("${Boost_INCLUDE_DIRS}" )
include_directories(${libuv_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})
include_directories("${Charm_DIR}")


add_compile_options($<$<CXX_COMPILER_ID:GNU>:-Wno-narrowing>)
configure_file( cmake_variables.h.in ${CMAKE_BINARY_DIR}/generated/cmake_variables.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ ) # Make sure it can be included...
add_executable (${PROJECT_NAME} "PacketSender.cpp"  ${Charm_DIR}/Mcpd8.CmdPacket.cpp ${Charm_DIR}/Mesyec.Mpsd8.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE  ${Boost_LIBRARIES} uv_a ) # libstdc++.a libm.a libc.a
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>  )

# TODO: Add tests and install targets if needed.
target_compile_options(${PROJECT_NAME} PRIVATE
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/W3>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/wd4996>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/GS>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/EHsc>>
)

if(UNIX AND NOT APPLE)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andreas Langhoff") # required
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)

endif()

if(NOT INSTALL_DIR)
if(WIN32)
set(INSTALL_DIR $ENV{HOMEDRIVE}$ENV{HOMEPATH}) # or $ENV{USERPROFILE}
else()
set(INSTALL_DIR ~/)
endif()
endif()

if(WIN32)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
set(WIN32LIB_SUFFIX d)
set(BOOSTLIB_DBG -gd)
endif()
  install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION ${INSTALL_DIR}
        )
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/boost_filesystem-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_thread-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_chrono-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_locale-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"


       DESTINATION ${INSTALL_DIR}
        )
	if(${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR} VERSION_LESS "1_73")
   install(
   FILES
      "${CMAKE_CURRENT_BINARY_DIR}/boost_date_time-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    DESTINATION ${INSTALL_DIR}
   )
   endif()

else()
 install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION ${INSTALL_DIR}
        )
endif()

