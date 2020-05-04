# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.16)
project(charm)

 add_compile_definitions(SIMPLE_LOGGER)
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
     add_compile_definitions(WIN32_PRE10OLDSTYLE)
endif()
  
add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
set(CMAKE_CXX_STANDARD 17) 
#find_package (Python COMPONENTS Interpreter Development NumPy)

#set(Boost_USE_STATIC_LIBS,ON)
#set(Boost_USE_STATIC_RUNTIME,ON)

find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread program_options iostreams log log_setup) # header only libraries must not be added here
#find_path(Boost_INCLUDE_DIR boost)

message(********cmake_toolchain=  ${CMAKE_TOOLCHAIN_FILE}             ***********)
#message(*** is debug==  ${$<$<CONFIG:DEBUG>:-gd>>} ****************)
if(DEFINED Protobuf_DIR )
#sort of hack for snapcraft.yaml build
find_package(Protobuf REQUIRED PATHS ${Protobuf_DIR})
endif()
#set ("OpenCV_DIR" "${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/opencv")
message(************  ${OpenCV_DIR}  *************)
#find_package(OpenCV  REQUIRED PATHS  ${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/opencv)
find_package(OpenCV  REQUIRED)
find_package(unofficial-libuv CONFIG REQUIRED)

#set (magic_enum_DIR ${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/magic_enum)

message(******************** ${magic_enum_DIR}   ************)
find_package(magic_enum CONFIG REQUIRED )


include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
include_directories(${ASIO_EXTENSIONS_PATH})
include_directories(./)
include_directories(${SIGSLOT_PATH})
#include_directories(${_Python_INCLUDE_DIR})
# Add source to this project's executable.
add_executable (${PROJECT_NAME} "mesyDeviceCtrl.cpp"  "OptionPrinter.cpp" "CustomOptionDescription.cpp" "simpleLogger.cpp" "coloured_console_sink.cpp" "Zweistein.ping.cpp" ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum ${Boost_LIBRARIES} ${OpenCV_LIBS} unofficial::libuv::libuv )
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>  )


ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
           ${CMAKE_CURRENT_BINARY_DIR}/_version.cpp
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)


if(WIN32)
  install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION $ENV{USERPROFILE}
        )
  install(FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/boost_filesystem-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_thread-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll" 
    "${CMAKE_CURRENT_BINARY_DIR}/boost_chrono-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll" 
    "${CMAKE_CURRENT_BINARY_DIR}/boost_program_options-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_iostreams-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_regex-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_log-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_date_time-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_highgui.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_imgproc.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_core.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_imgcodecs.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_videoio.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/libuv.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/bz2.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/jpeg62.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/libpng16.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/lzma.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/tiff.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/webp.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/zlib1.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/zstd.dll"
       DESTINATION $ENV{USERPROFILE}
        )
else()
 install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION ~/
        )
endif()
 
