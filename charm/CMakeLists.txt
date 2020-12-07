# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)
project(charm)
if(NOT Charm_DIR)
set(Charm_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()


get_filename_component(parentdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY )

if(CMAKE_BUILD_TYPE)
    set(BUILDVARIANT ${CMAKE_BUILD_TYPE})
    else()
    set(BUILDVARIANT Release)
    endif()

 add_definitions(-DSIMPLE_LOGGER)
if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWIN32_PRE10OLDSTYLE)
    set(OpenCV_DIR ${parentdir}/opencv/out/${BUILDVARIANT}/install)
else()
    set(OpenCV_DIR ${parentdir}/opencv/out/${BUILDVARIANT}/install/lib/cmake/opencv4)
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

find_package(OpenCV  REQUIRED)
message(******OpenCV_Dir=   ${OpenCV_DIR}  *************)

include_directories(${libuv_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/asio-extensions/include)



# Add source to this project's executable.
add_executable (${PROJECT_NAME} "mesyDeviceCtrl.cpp" "Mesytec.Mcpd8.cpp" "Mcpd8.Parameters.cpp" "OptionPrinter.cpp" "CustomOptionDescription.cpp" "simpleLogger.cpp" "coloured_console_sink.cpp" "Zweistein.ping.cpp" "version.cpp" "Mcpd8.CmdPacket.cpp" "Mesyec.Mpsd8.cpp" "Mesytec.listmode.write.cpp" "Mesytec.listmode.cpp" "MesytecSystem.Data.cpp" "Zweistein.GetConfigDir.cpp" "Mesytec.config.cpp" "Charm.System.cpp" "Zweistein.XYDetectorSystem.cpp")

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
target_link_libraries(${PROJECT_NAME} PRIVATE   ${Boost_LIBRARIES} ${OpenCV_LIBS}  uv_a )


if(NOT INSTALL_DIR)
if(WIN32)
set(INSTALL_DIR  $ENV{USERPROFILE} ) # or $ENV{HOMEDRIVE}$ENV{HOMEPATH}
else()
set(INSTALL_DIR /usr/local/bin)
endif()
endif()

set (CONFIG_FILES
 "charm.json"
 "erwin.json"
 "mesytec.json"
 "sans1.json"
 "pos_cal_lut_2016_07_13.txt"
 )


 message("INSTALL_DIR = ${INSTALL_DIR}")

install(TARGETS ${PROJECT_NAME}
        RUNTIME  DESTINATION ${INSTALL_DIR} COMPONENT config
        )


if(WIN32)
  file(TO_CMAKE_PATH $ENV{USERPROFILE} CINSTDIR)
set(CDESTDIR ${CINSTDIR}/.CHARMing)
message(${CDESTDIR})
foreach(CURFILE ${CONFIG_FILES})
 file(TO_CMAKE_PATH ${CDESTDIR}/${CURFILE} CPATH)
 install(CODE "
 if (EXISTS ${CPATH})
  MESSAGE(EXIST: ${CPATH})
  else()
  MESSAGE(NOT_EXISTS: ${CPATH})
  file(INSTALL ${CMAKE_CURRENT_SOURCE_DIR}/${CURFILE} DESTINATION ${CDESTDIR} )
 endif()
 "
 WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX}
 COMPONENT config
 )
endforeach()

 else()

 set(CDESTDIR /etc/CHARMing)
message(${CDESTDIR})
foreach(CURFILE ${CONFIG_FILES})
 file(TO_CMAKE_PATH ${CDESTDIR}/${CURFILE} CPATH)
 install(CODE "
 if (EXISTS ${CPATH})
  MESSAGE(EXIST: ${CPATH})
  else()
  MESSAGE(NOT_EXISTS: ${CPATH})
  file(INSTALL ${CMAKE_CURRENT_SOURCE_DIR}/${CURFILE} DESTINATION ${CDESTDIR} )
 endif()
 "
 WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX}
 COMPONENT config

 )
endforeach()

 install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/charm-sudo.sh"
    DESTINATION ${INSTALL_DIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
   install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/entangle-server-privileged.sh"
    DESTINATION /usr/bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()

