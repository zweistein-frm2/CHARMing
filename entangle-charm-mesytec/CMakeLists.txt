# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)

set(PROJECT1 mesytecsystem)
set(PROJECT2 listmodereplay)
set(PROJECT3 charmsystem)

set(repository ${Charm_DIR})

set(Boost_USE_STATIC_LIBS ON)

foreach(PROJECT_NAME IN LISTS PROJECT1 PROJECT2 PROJECT3)
    message(STATUS "PROJECT_NAME=${PROJECT_NAME}")

project(${PROJECT_NAME})
add_compile_definitions(ENTANGLE_LOGGER)
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
else()
message("on linux boost >= 1.71 must be installed from source with speficic compile settings (static link, no icu")

#set(BOOST_ROOT /usr )
#set(Boost_INCLUDE_DIR ${BOOST_ROOT}/include)
endif()

add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
add_compile_definitions(BOOST_CHRONO_VERSION=2)
set(CMAKE_CXX_STANDARD 17)

find_package (Python COMPONENTS Interpreter Development NumPy)

if(NOT ENTANGLE_ROOT)
    if(WIN32)
      message(**   "python lib is here :"  ${Python_SITELIB})
    else()
      execute_process(COMMAND  find /usr/local/lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/dist-packages/ -type d -name "entangle-*.egg"  OUTPUT_VARIABLE ENTANGLE_ROOT ERROR_QUIET)
      if(ENTANGLE_ROOT)
        set(ENTANGLE_RES /etc/entangle)  # if standard install .res should go there (or one could parse /etc/entangle/entangle.conf.dist to know better)
      endif()
    endif()

else()
 if(NOT ENTANGLE_RES)
    set(ENTANGLE_RES ${ENTANGLE_ROOT}/example)
 endif()
endif()
message("ENTANGLE_ROOT=" ${ENTANGLE_ROOT} )
message("ENTANGLE_RES=" ${ENTANGLE_RES} )

find_package(Boost 1.71 REQUIRED COMPONENTS python${Python_VERSION_MAJOR}${Python_VERSION_MINOR} numpy${Python_VERSION_MAJOR}${Python_VERSION_MINOR} filesystem chrono thread iostreams) # header only libraries must not be added here
find_path(Boost_INCLUDE_DIR boost)

message(********************Using Boost :${Boost_INCLUDE_DIR}  ******************************)
find_package(OpenCV REQUIRED)


#find_package(unofficial-libuv CONFIG REQUIRED PATHS ${Charm_DIR}/libuv )

#set (magic_enum_DIR ${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/magic_enum)
#message("**magic_enum_DIR=" ${magic_enum_DIR}   ************)
#find_package(magic_enum CONFIG REQUIRED )

include_directories(${libuv_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})
include_directories(${_Python_INCLUDE_DIR})
include_directories(${Charm_DIR})
include_directories(${Charm_DIR}/asio-extensions/include)
include_directories(${PacketSender_DIR})
include_directories(pyboostcvconverter/include)
# Add source to this project's executable.
include_directories(${_Python_NumPy_INCLUDE_DIR})
set(Boost_python_library Boost_PYTHON${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_LIBRARY_RELEASE )
set(Boost_numpy_library Boost_NUMPY${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_LIBRARY_RELEASE )
message(**Boost_python_library********** ${Boost_python_library}*********************)
message(****resolves_to***************** ${${Boost_python_library}}**************************)
message(**Boost_numpy_library********** ${Boost_numpy_library}*********************)
message(****resolves_to***************** ${${Boost_numpy_library}}**************************)





add_library(${PROJECT_NAME} SHARED )

target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} ${OpenCV_LIBS} uv_a  )
target_link_libraries(${PROJECT_NAME} PRIVATE ${Python_LIBRARY_RELEASE} ${${Boost_python_library}} ${${Boost_numpy_library}})

if(MSVC)
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
else()
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif()

add_custom_command(
    OUTPUT ${repository}/version.cpp
           ${repository}/_version.cpp
    COMMAND ${CMAKE_COMMAND} -P
            ${repository}/version.cmake
            WORKING_DIRECTORY ${repository}
            )


add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
         COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/charm.py
                ${CMAKE_CURRENT_BINARY_DIR}/charm.py
         COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/mesytec.py
                ${CMAKE_CURRENT_BINARY_DIR}/mesytec.py
        COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_CURRENT_SOURCE_DIR}/mesytec-replay.py
              ${CMAKE_CURRENT_BINARY_DIR}/mesytec-replay.py
         COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py
              ${CMAKE_CURRENT_BINARY_DIR}/__init__.py

        COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_CURRENT_SOURCE_DIR}/msmtsystem.py
              ${CMAKE_CURRENT_BINARY_DIR}/msmtsystem.py

         COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_CURRENT_SOURCE_DIR}/core.py
              ${CMAKE_CURRENT_BINARY_DIR}/core.py
         COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_CURRENT_SOURCE_DIR}/settings.py
              ${CMAKE_CURRENT_BINARY_DIR}/settings.py
         COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/simulator.py
                ${CMAKE_CURRENT_BINARY_DIR}/simulator.py
         COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_CURRENT_SOURCE_DIR}/test-replay.py
              ${CMAKE_CURRENT_BINARY_DIR}/test-replay.py

        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/test.py
                ${CMAKE_CURRENT_BINARY_DIR}/test.py

              )

target_sources(${PROJECT_NAME}
PRIVATE
${PROJECT_NAME}.cpp
pyboostcvconverter/src/pyboost_cv2_converter.cpp
pyboostcvconverter/src/pyboost_cv3_converter.cpp
pyboostcvconverter/src/pyboost_cv4_converter.cpp
${Charm_DIR}/Entangle.Logger.cpp
${Charm_DIR}/Zweistein.ping.cpp
${Charm_DIR}/Mesytec.Mcpd8.cpp
${Charm_DIR}/Mcpd8.Parameters.cpp
${Charm_DIR}/Mcpd8.CmdPacket.cpp
${Charm_DIR}/Mesyec.Mpsd8.cpp
${Charm_DIR}/Mesytec.listmode.write.cpp
${Charm_DIR}/Mesytec.listmode.cpp
${Charm_DIR}/MesytecSystem.Data.cpp
${Charm_DIR}/Zweistein.GetConfigDir.cpp
${Charm_DIR}/Mesytec.config.cpp
${Charm_DIR}/Zweistein.XYDetectorSystem.cpp
${repository}/version.cpp
)

if(ENTANGLE_ROOT )
 install(
    TARGETS ${PROJECT_NAME}
      RUNTIME DESTINATION ${ENTANGLE_ROOT}/entangle/device/charming
      LIBRARY DESTINATION ${ENTANGLE_ROOT}/entangle/device/charming
 )
else()
install(CODE "MESSAGE(\"${PROJECT_NAME} NOT INSTALLED\")")
install(CODE "MESSAGE(\"REASON: prerequisite Entangle NOT INSTALLED\")")
install(CODE "MESSAGE(\"https://forge.frm2.tum.de/entangle/doc/entangle-master/build/\")")

endif()

endforeach()

if(ENTANGLE_ROOT )
 install(
  FILES
   ${CMAKE_CURRENT_SOURCE_DIR}/charm.py
    ${CMAKE_CURRENT_SOURCE_DIR}/mesytec.py
   ${CMAKE_CURRENT_SOURCE_DIR}/mesytec-replay.py
    ${CMAKE_CURRENT_SOURCE_DIR}/msmtsystem.py
    ${CMAKE_CURRENT_SOURCE_DIR}/__init__.py
   ${CMAKE_CURRENT_SOURCE_DIR}/core.py
   ${CMAKE_CURRENT_SOURCE_DIR}/settings.py
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator.py
   ${CMAKE_CURRENT_SOURCE_DIR}/test-replay.py
   ${CMAKE_CURRENT_SOURCE_DIR}/test.py
   DESTINATION ${ENTANGLE_ROOT}/entangle/device/charming
 )
 if(UNIX AND NOT APPLE)
 install(
  FILES
   ${CMAKE_CURRENT_SOURCE_DIR}/privileged.sh
    DESTINATION ${ENTANGLE_ROOT}
 )
 endif()
 if(WIN32)
   if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
     set(WIN32LIB_SUFFIX d)
     set(BOOSTLIB_DBG -gd)
   endif()
   install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/boost_filesystem-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_thread-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_chrono-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_iostreams-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_numpy${Python_VERSION_MAJOR}${Python_VERSION_MINOR}-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/boost_python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}-vc${MSVC_TOOLSET_VERSION}-mt-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/opencv_core${WIN32LIB_SUFFIX}.dll"
    "${libuv_BINARY_DIR}/uv.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/bz2${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/lzma${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/zlib1${WIN32LIB_SUFFIX}.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/zstd${WIN32LIB_SUFFIX}.dll"
    DESTINATION ${ENTANGLE_ROOT}/entangle/device/charming
   )
   endif()
  if(${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR} VERSION_LESS "1_73")
   install(
   FILES
      "${CMAKE_CURRENT_BINARY_DIR}/boost_date_time-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    DESTINATION ${ENTANGLE_ROOT}/entangle/device/charming
   )
   endif()
endif()

add_subdirectory (entangle)