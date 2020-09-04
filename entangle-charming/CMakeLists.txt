# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)

set(PROJECT1 mesytecsystem)
set(PROJECT2 listmodereplay)
set(PROJECT3 charmsystem)
set(PROJECT4 DebugHelper)

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

set(ENTANGLE_FILES "")
list(APPEND ENTANGLE_FILES
  [device/charming]
  )

#foreach(PROJECT_NAME IN LISTS PROJECT4 )
foreach(PROJECT_NAME IN LISTS PROJECT1 PROJECT2 PROJECT3 PROJECT4)
    message(STATUS "PROJECT_NAME=${PROJECT_NAME}")

project(${PROJECT_NAME})

if(${PROJECT_NAME} STREQUAL ${PROJECT1})
   add_subdirectory(${parentdir}/libuv ${CMAKE_CURRENT_BINARY_DIR}/libuv)

endif()
target_compile_options(uv_a PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fPIC>)
set(libuv_INCLUDE_DIR ${parentdir}/libuv/include)

add_compile_definitions(ENTANGLE_LOGGER)
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
  #  set (Python_VERSION_MAJOR 3)
  #  set(Python_VERSION_MINOR 7)
  # set(Python_FIND_ABI "OFF" "ANY" "ANY")

else()
  message("on linux boost >= 1.71 must be installed from source with speficic compile settings (static link, no icu")

  #set(BOOST_ROOT /usr )
  #set(Boost_INCLUDE_DIR ${BOOST_ROOT}/include)
endif()

add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)

set(CMAKE_CXX_STANDARD 17)


#find_package (Python "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}" EXACT REQUIRED COMPONENTS Interpreter Development NumPy)
find_package (Python  REQUIRED COMPONENTS Interpreter Development NumPy)

message(STATUS "Python: version=${Python3_VERSION} interpreter=${Python3_EXECUTABLE}")
message(STATUS "Include Dirs=${Python_INCLUDE_DIRS} version=${Python_VERSION}")

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

find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread  iostreams  python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}
            numpy${Python_VERSION_MAJOR}${Python_VERSION_MINOR} locale  ${BOOST_PATHS}) # header only libraries must not be added here

include_directories("${Boost_INCLUDE_DIRS}" )

find_package(OpenCV REQUIRED)

include_directories(${libuv_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})
include_directories(${_Python_INCLUDE_DIR})
include_directories(${Charm_DIR})
include_directories(${Charm_DIR}/asio-extensions/include)
include_directories(${PacketSender_DIR})
include_directories(pyboostcvconverter/include)
# Add source to this project's executable.
include_directories(${_Python_NumPy_INCLUDE_DIR})
set(Boost_python_library Boost_PYTHON${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_LIBRARY_DEBUG )
set(Boost_numpy_library Boost_NUMPY${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_LIBRARY_DEBUG )
message(**Boost_python_library********** ${Boost_python_library}*********************)
message(****resolves_to***************** ${${Boost_python_library}}**************************)
message(**Boost_numpy_library********** ${Boost_numpy_library}*********************)
message(****resolves_to***************** ${${Boost_numpy_library}}**************************)


if(${PROJECT_NAME} STREQUAL "DebugHelper")
message(***DebugHelper executable********)

message(**Python Lib: ${Python_LIBRARIES} ****)


add_executable(${PROJECT_NAME}  )
target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/Od>>
  $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/GS>>
 )
#target_include_directories(${PROJECT_NAME} PRIVATE ${libuv_INCLUDE_DIR} )
#target_link_options(${PROJECT_NAME}   PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc> )
target_link_libraries(${PROJECT_NAME} PRIVATE  ${Python_LIBRARY_RELEASE} ${Boost_LIBRARIES} ${OpenCV_LIBS}  uv_a )

else()

add_library(${PROJECT_NAME} SHARED )

target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} ${OpenCV_LIBS} uv_a  )
target_link_libraries(${PROJECT_NAME} PRIVATE ${Python_LIBRARY_RELEASE} ${${Boost_python_library}} ${${Boost_numpy_library}})

if(MSVC)
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
else()
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif()




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

if(NOT ${PROJECT_NAME} STREQUAL "DebugHelper")
get_target_property(tmpSUFFIX ${PROJECT_NAME} SUFFIX)

if(WIN32)
set(tmpSUFFIX ".pyd")
else()
set(tmpSUFFIX ".so")
endif()

get_target_property(tmpPREFIX ${PROJECT_NAME} PREFIX)

if(NOT DEFINED tmpPREFIX)
  set(tmpPREFIX "")
  elseif( "${tmpPREFIX}" STREQUAL "tmpPREFIX-NOTFOUND")
  set(tmpPREFIX "")
 endif()

 set(tmpPREFIX "") #always , otherwise trouble with cross compile vs for linux

list(APPEND ENTANGLE_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${tmpPREFIX}${PROJECT_NAME}${tmpSUFFIX}
  )

endif()

endforeach()

list(APPEND ENTANGLE_FILES
   [${CMAKE_CURRENT_SOURCE_DIR}]
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
)

list(APPEND ENTANGLE_FILES
   [/etc/entangle/example]
    ${CMAKE_CURRENT_SOURCE_DIR}/entangle/example/charm-replay.res
    ${CMAKE_CURRENT_SOURCE_DIR}/entangle/example/charm-SANS1.res
 )
set(txt ${ENTANGLE_FILES})
string (REPLACE ";" "\n" txt "${txt}")
message(${txt})
#FILE(REMOVE  ${parentdir}/ENTANGLE_FILES)
FILE(WRITE ${parentdir}/ENTANGLE_FILES ${txt})
message(written to : ${parentdir}/ENTANGLE_FILES)



if(ENTANGLE_ROOT )
 install(
  FILES
  ${ENTANGLE_FILES}
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

  if(${Boost_VERSION_MAJOR}.${Boost_VERSION_MINOR} VERSION_LESS 1.73)
   message("${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}  INSTALL EXTRA")
   install(
   FILES
      "${CMAKE_CURRENT_BINARY_DIR}/boost_date_time-vc${MSVC_TOOLSET_VERSION}-mt${BOOSTLIB_DBG}-x64-${Boost_VERSION_MAJOR}_${Boost_VERSION_MINOR}.dll"
    DESTINATION ${ENTANGLE_ROOT}/entangle/device/charming
   )
  else()
  endif()
endif()

add_subdirectory (entangle)