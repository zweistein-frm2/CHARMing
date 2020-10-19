# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)
project(charm)
if(NOT Charm_DIR)
set(Charm_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()



 add_definitions(-DSIMPLE_LOGGER)
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWIN32_PRE10OLDSTYLE)
    set(OpenCV_DIR "C:\\opencv\\build")
    set(OpenCV_STATIC ON) # no effect
endif()

add_definitions(-DASIOEXT_USE_BOOST_ASIO)

set(CMAKE_CXX_STANDARD 17)

#find_package (Python COMPONENTS Interpreter Development NumPy)

set(Boost_USE_STATIC_LIBS ON)


find_package(Boost ${boost_currentversion} REQUIRED COMPONENTS filesystem chrono thread program_options iostreams log log_setup locale ${BOOST_PATHS}) # header only libraries must not be added here

#message(*** is debug==  ${$<$<CONFIG:DEBUG>:-gd>>} ****************)

find_package(OpenCV  REQUIRED)
message(******OpenCV_Dir=   ${OpenCV_DIR}  *************)

include_directories(${libuv_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/asio-extensions/include)



# Add source to this project's executable.
add_executable (${PROJECT_NAME} "mesyDeviceCtrl.cpp" "Mesytec.Mcpd8.cpp" "Mcpd8.Parameters.cpp" "OptionPrinter.cpp" "CustomOptionDescription.cpp" "simpleLogger.cpp" "coloured_console_sink.cpp" "Zweistein.ping.cpp" "version.cpp" "Mcpd8.CmdPacket.cpp" "Mesyec.Mpsd8.cpp" "Mesytec.listmode.write.cpp" "Mesytec.listmode.cpp" "MesytecSystem.Data.cpp" "Zweistein.GetConfigDir.cpp" "Mesytec.config.cpp" "Charm.System.cpp" "Zweistein.XYDetectorSystem.cpp")

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>)
target_link_libraries(${PROJECT_NAME} PRIVATE   ${Boost_LIBRARIES} ${OpenCV_LIBS}  uv_a )
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:libstdc++.a>)



if(NOT INSTALL_DIR)
if(WIN32)
set(INSTALL_DIR  $ENV{USERPROFILE} ) # or $ENV{HOMEDRIVE}$ENV{HOMEPATH}
else()
set(INSTALL_DIR /usr/local/bin)
endif()
endif()

set (CONFIG_FILES
 "charm.json"
 "mesytec.json"
 "sans1.json"
 "pos_cal_lut_2016_07_13.txt"
 )

if(WIN32)
if(DEFINED CMAKE_BUILD_TYPE)
 if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(WIN32LIB_SUFFIX d)
  set(BOOSTLIB_DBG -gd)
 endif()
endif()
  install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION ${INSTALL_DIR}
        )
  install(FILES ${CONFIG_FILES}
  DESTINATION "$ENV{USERPROFILE}/.CHARMing"
  )
  if(${Boost_VERSION_MAJOR}.${Boost_VERSION_MINOR} VERSION_LESS 1.75)
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
    else()
    install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_world440${WIN32LIB_SUFFIX}.dll"
     DESTINATION ${INSTALL_DIR}
        )
    endif()
	if(${Boost_VERSION_MAJOR}.${Boost_VERSION_MINOR} VERSION_LESS 1.73)
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
install(FILES ${CONFIG_FILES}
  DESTINATION /etc/CHARMing
  )
 install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/charm-sudo.sh"
    DESTINATION ${INSTALL_DIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/entangle-privileged.sh"
    DESTINATION /usr/bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()

