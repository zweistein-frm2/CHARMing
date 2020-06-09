# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.16)
project(charm)
if(NOT Charm_DIR)
set(Charm_DIR ${PROJECT_SOURCE_DIR})
endif()
set(repository ${Charm_DIR})
message(****"repository=" ${repository})

include(version.cmake)
message(---------> git Date : ${GIT_DATE})
set(CMAKE_PROJECT_VERSION_MAJOR ${GIT_LATEST_TAG})
set(CMAKE_PROJECT_VERSION_MINOR ${GIT_NUMBER_OF_COMMITS_SINCE})
set(CMAKE_PROJECT_VERSION_PATCH ${GIT_REV}-${GIT_DATE})


 add_compile_definitions(SIMPLE_LOGGER)
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
     add_compile_definitions(WIN32_PRE10OLDSTYLE)
endif()
  
add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
add_compile_definitions(BOOST_CHRONO_VERSION=2)

set(CMAKE_CXX_STANDARD 17) 

#find_package (Python COMPONENTS Interpreter Development NumPy)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread program_options iostreams log log_setup locale) # header only libraries must not be added here
#find_path(Boost_INCLUDE_DIR boost)

message(********cmake_toolchain=  ${CMAKE_TOOLCHAIN_FILE}             ***********)
#message(*** is debug==  ${$<$<CONFIG:DEBUG>:-gd>>} ****************)
if(DEFINED Protobuf_DIR )
#sort of hack for snapcraft.yaml build
find_package(Protobuf REQUIRED PATHS ${Protobuf_DIR})
endif()
#set ("OpenCV_DIR" "${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/opencv")

find_package(OpenCV  REQUIRED)
message(******OpenCV_Dir=   ${OpenCV_DIR}  *************)

find_package(unofficial-libuv CONFIG REQUIRED)

#set (magic_enum_DIR ${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/magic_enum)

message(******magic_enum_DIR= ${magic_enum_DIR}   ************)
find_package(magic_enum CONFIG REQUIRED )

message(**CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR})
message(**CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR})

include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
include_directories(${ASIO_EXTENSIONS_PATH})
include_directories(./)

include_directories(${SIGSLOT_PATH})
#include_directories(${_Python_INCLUDE_DIR})
# Add source to this project's executable.
add_executable (${PROJECT_NAME} "mesyDeviceCtrl.cpp" "Mesytec.Mcpd8.cpp" "Mcpd8.Parameters.cpp" "OptionPrinter.cpp" "CustomOptionDescription.cpp" "simpleLogger.cpp" "coloured_console_sink.cpp" "Zweistein.ping.cpp" ${repository}/version.cpp "Mcpd8.CmdPacket.cpp" "Mesyec.Mpsd8.cpp" "Mesytec.listmode.write.cpp" "Mesytec.listmode.cpp" "MesytecSystem.Data.cpp" "Zweistein.GetConfigDir.cpp" "Mesytec.config.cpp")

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
#target_link_options(${PROJECT_NAME}   PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc> )

target_link_libraries(${PROJECT_NAME} PRIVATE  magic_enum::magic_enum ${Boost_LIBRARIES} ${OpenCV_LIBS}  unofficial::libuv::libuv )


add_custom_command(
    OUTPUT ${repository}/version.cpp
           ${repository}/_version.cpp
    COMMAND ${CMAKE_COMMAND} -P 
            ${repository}/version.cmake
            WORKING_DIRECTORY ${repository}
            )




if(UNIX AND NOT APPLE)
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andreas Langhoff") # required
set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})

SET (CPACK_DEBIAN_PACKAGE_DEPENDS "libopencv-dev")
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
    "${CMAKE_CURRENT_BINARY_DIR}/boost_program_options-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_iostreams-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_regex-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_log-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_locale-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_highgui${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_imgproc${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_core${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_imgcodecs${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_videoio${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/libuv.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/bz2${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/jpeg62.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/libpng16${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/lzma${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/tiff${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/webp${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/zlib${WIN32LIB_SUFFIX}1.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/zstd${WIN32LIB_SUFFIX}.dll"
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
 
