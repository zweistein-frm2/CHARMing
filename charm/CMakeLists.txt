# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)
project(charm)
if(NOT Charm_DIR)
set(Charm_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()


get_filename_component(parentdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY )

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE "Release")
endif()

 add_definitions(-DSIMPLE_LOGGER)
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWIN32_PRE10OLDSTYLE)
    set(OpenCV_DIR ${parentdir}/opencv/out/${CMAKE_BUILD_TYPE}/install)
else()
    set(OpenCV_DIR ${parentdir}/opencv/out/${CMAKE_BUILD_TYPE}/install/lib64/cmake/opencv4)
    if(NOT IS_DIRECTORY ${OpenCV_DIR})
        set(OpenCV_DIR ${parentdir}/opencv/out/${CMAKE_BUILD_TYPE}/install/lib/cmake/opencv4)
    endif()
endif()
#set(OpenCV_DIR "C:\\opencv\\build")
set(OpenCV_STATIC ON) # no effect

message(OpenCV_DIR=${OpenCV_DIR})
add_definitions(-DASIOEXT_USE_BOOST_ASIO)

set(CMAKE_CXX_STANDARD 17)

#find_package (Python COMPONENTS Interpreter Development NumPy)

set(Boost_USE_STATIC_LIBS ON)


find_package(Boost ${boost_currentversion} REQUIRED COMPONENTS filesystem chrono thread program_options iostreams log log_setup locale ${BOOST_PATHS}) # header only libraries must not be added here

#message(*** is debug==  ${$<$<CONFIG:DEBUG>:-gd>>} ****************)

find_package(OpenCV  REQUIRED PATHS ${OpenCV_DIR})
message(******OpenCV_Dir=   ${OpenCV_DIR}  *************)

include_directories(${libuv_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/asio-extensions/include)

IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
   set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")


# Add source to this project's executable.
add_executable (${PROJECT_NAME} "mesyDeviceCtrl.cpp" "Mesytec.Mcpd8.cpp" "Mcpd8.Parameters.cpp" "OptionPrinter.cpp" "CustomOptionDescription.cpp" "simpleLogger.cpp" "coloured_console_sink.cpp" "Zweistein.ping.cpp" "version.cpp" "Mcpd8.CmdPacket.cpp" "Mesyec.Mpsd8.cpp" "Mesytec.listmode.write.cpp" "Mesytec.listmode.cpp" "MesytecSystem.Data.cpp" "Zweistein.GetConfigDir.cpp" "Mesytec.config.cpp" "Charm.System.cpp" "Zweistein.XYDetectorSystem.cpp" "glibc2.27_compat2.2.5.cpp")

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
target_link_libraries(${PROJECT_NAME} PRIVATE   ${Boost_LIBRARIES} ${OpenCV_LIBS}  uv_a )
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>)
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>)

if(NOT INSTALL_DIR)
if(WIN32)
set(INSTALL_DIR  $ENV{USERPROFILE} ) # or $ENV{HOMEDRIVE}$ENV{HOMEPATH}
else()
set(INSTALL_DIR /usr/local/bin)
endif()
endif()

set (CONFIG_FILES
 "erwin-small.json"
 "sans1.json"
 "pos_cal_lut_2016_07_13.txt"
 )


 message("INSTALL_DIR = ${INSTALL_DIR}")

install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION ${INSTALL_DIR}
        )


if(WIN32)
 install(FILES ${CONFIG_FILES}
  DESTINATION "$ENV{USERPROFILE}/.CHARMing/examples"
  )
 else()
 install(FILES ${CONFIG_FILES}
  DESTINATION /etc/CHARMing/examples
  )
 install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/charm-sudo.sh"
    DESTINATION ${INSTALL_DIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/entangle-server-privileged.sh"
    DESTINATION /usr/bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()


set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst; ${CMAKE_CURRENT_SOURCE_DIR}/prerm ")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postinst")