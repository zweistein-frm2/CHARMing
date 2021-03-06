﻿# CMakeList.txt :
cmake_minimum_required (VERSION 3.11)

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.2.2)
  message(STATUS " gcc compiler bug.")
  message(FATAL_ERROR "https://bugzilla.redhat.com/show_bug.cgi?id=1853900")
endif()


#
# in ${CMAKE_CURRENT_SOURCE_DIR}/boost/jamroot or in ${CMAKE_CURRENT_SOURCE_DIR}/boost/CMakeLists.txt
# the actual boost version is contained and can be extracted to set boost_currentversion automatically

set(boost_currentversion 1.75.0)

set(CMAKE_VERBOSE_MAKEFILE ON)


if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE "Release")
endif()


set (Build_Type RELEASE)


if(DEFINED CMAKE_BUILD_TYPE)
 string(FIND ${CMAKE_BUILD_TYPE} "Release" isreleasebuild)
 message(STATUS ${CMAKE_BUILD_TYPE})
 if(isreleasbuild LESS 0)
  set (Build_Type DEBUG)
 endif()
 endif()

message(STATUS ${Build_Type})
set(PROJECT1 DebugHelper)
set(PROJECT2 mesytecsystem)
set(PROJECT3 listmodereplay)
set(PROJECT4 charmsystem)

get_filename_component(parentdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY )

if(DEFINED CHARMING_DIR)
  set(BOOST_PREFIX ${CHARMING_DIR}/boost/out)
  set( parentdir ${CHARMING_DIR})
else()
  set(BOOST_PREFIX ${parentdir}/boost/out)
endif()

message(parentdir=${parentdir})

include(${parentdir}/init.cmake)
#set(Boost_DEBUG ON)
#set(Boost_DETAILED_FAILURE_MSG ON)
#set(Boost_VERBOSE ON)
set(Boost_NO_SYSTEM_PATHS ON)


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
message(STATUS "BOOST_PREFIX=${BOOST_PREFIX}")

set(Charm_DIR ${parentdir}/charm )
set(repository ${Charm_DIR})

include(${repository}/version.cmake)

set(Boost_USE_STATIC_LIBS ON)

set(ENTANGLE_FILES "")
list(APPEND ENTANGLE_FILES
  [device/charming]
  )

set(ENTANGLE_FILES_DEVICE_CHARMING "")

#foreach(PROJECT_NAME IN LISTS PROJECT1 )
foreach(PROJECT_NAME IN LISTS PROJECT1 PROJECT2 PROJECT3 PROJECT4)
    message(STATUS "PROJECT_NAME=${PROJECT_NAME}")
    project(${PROJECT_NAME})

    if(${PROJECT_NAME} STREQUAL ${PROJECT1})
       add_subdirectory(${parentdir}/libuv/  ${CMAKE_CURRENT_BINARY_DIR}/libuv)
       set(libuv_INCLUDE_DIR "${parentdir}/libuv/include")
    endif()

    target_compile_options(uv_a PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fpic>)

    add_definitions(-DENTANGLE_LOGGER)
    #add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

    #add_definitions(-DUNICODE -D_UNICODE)
    if(WIN32)
        add_definitions(-D_WIN32_WINNT=0x0601)
        set(opencvbuildtype  ${CMAKE_BUILD_TYPE})
        if(${opencvbuildtype}  STREQUAL "RelWithDebInfo")
          set(opencvbuildtype "Release")
        endif()
        set(OpenCV_DIR ${parentdir}/opencv/out/${opencvbuildtype}/install)
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
    message(STATUS CMAKE_VERSION=${CMAKE_VERSION})

    if(${CMAKE_VERSION}   VERSION_GREATER "3.11.4" )
    find_package (Python3  REQUIRED COMPONENTS Interpreter Development NumPy)
    else()
    message("ATTENTION OLD CMAKE: ${CMAKE_VERSION}")
    endif()
    message(STATUS "Python3: version=${Python3_VERSION} interpreter=${Python3_EXECUTABLE}")
    message(STATUS "Include Dirs=${Python3_INCLUDE_DIRS}")
    message(STATUS "Sitelib Dirs=${Python3_SITELIB}")
    message(STATUS "Python3_NumPy_INCLUDE_DIRS = ${Python3_NumPy_INCLUDE_DIRS}")
    message(STATUS "Python3_VERSION_MAJOR = ${Python3_VERSION_MAJOR}")
    message(STATUS "Python3_VERSION_MINOR = ${Python3_VERSION_MINOR}")
    message(STATUS "_Python3_LIBRARY_RELEASE = ${_Python3_LIBRARY_RELEASE}")
    if(NOT ENTANGLE_ROOT)
     if(UNIX)
         execute_process(COMMAND  find ${Python3_SITELIB} -type d -name "entangle"  OUTPUT_VARIABLE ENTANGLE_ROOT ERROR_QUIET)
     endif()
    endif()

    if(ENTANGLE_ROOT)
       if(NOT ENTANGLE_RES)
         if(UNIX)
            set(ENTANGLE_RES /etc/entangle)  # if standard install .res should go there (or one could parse /etc/entangle/entangle.conf.dist to know better)
         else()
          get_filename_component(entangledevroot ${ENTANGLE_ROOT} DIRECTORY )
           set (ENTANGLE_RES ${entangledevroot}/example)

         endif()
       endif()
    endif()

    message(STATUS "ENTANGLE_ROOT=" ${ENTANGLE_ROOT} )
    message(STATUS "ENTANGLE_RES=" ${ENTANGLE_RES} )


    find_package(Boost ${boost_currentversion} REQUIRED COMPONENTS filesystem chrono thread  iostreams  python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}
                numpy${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR} locale  ${BOOST_PATHS}) # header only libraries must not be added here

    include_directories(${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS} ${Boost_INCLUDE_DIR} )

    find_package(OpenCV REQUIRED PATHS ${OpenCV_DIR})

    include_directories(${libuv_INCLUDE_DIR})
    include_directories(${Charm_DIR})
    include_directories(${Charm_DIR}/asio-extensions/include)
    include_directories(${PacketSender_DIR})
    include_directories(pyboostcvconverter/include)
    # Add source to this project's executable.

    set(Boost_python_library Boost_PYTHON${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}_LIBRARY_${Build_Type} )
    set(Boost_numpy_library Boost_NUMPY${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}_LIBRARY_${Build_Type} )
    message(STATUS "Boost_python_library : ${${Boost_python_library}}")
    message(STATUS "Boost_numpy_library : ${${Boost_numpy_library}}")
    message(STATUS "_Python3_LIBRARY_RELEASE= ${_Python3_LIBRARY_${Build_Type}}")

   message(STATUS "CMAKE_INTDIR=${CMAKE_INTDIR}")

   IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
   set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
   ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

   if(${PROJECT_NAME} STREQUAL "DebugHelper")
     message(STATUS "add_executable(${PROJECT_NAME})")

	 IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
     set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -lpython3.7m")
     ENDIF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")

     add_executable(${PROJECT_NAME})

     target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)

     target_compile_options(${PROJECT_NAME} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/Od>>
      $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/GS>>
     )
     target_link_libraries (${PROJECT_NAME} PRIVATE "${_Python3_LIBRARY_${Build_Type}}" ${Boost_LIBRARIES} ${OpenCV_LIBS}  uv_a )

    else()
     message(STATUS "add_library(${PROJECT_NAME} SHARED )")
	 
	 
	 
     add_library(${PROJECT_NAME} SHARED )
	 
     if(WIN32)
	  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
      set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "" FORCE )
	  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "" FORCE)
     else()
      set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
     endif()
     target_compile_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fpic>)
     target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES} ${OpenCV_LIBS} uv_a  )
     target_link_libraries(${PROJECT_NAME} PRIVATE "${_Python3_LIBRARY_${Build_Type}}" ${${Boost_python_library}} ${${Boost_numpy_library}})

     
    endif()
    #set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " -O3 -DNDEBUG")
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
                  ${CMAKE_CURRENT_SOURCE_DIR}/msmtsystem.py
                  ${CMAKE_CURRENT_BINARY_DIR}/msmtsystem.py

             COMMAND ${CMAKE_COMMAND} -E copy
                  ${CMAKE_CURRENT_SOURCE_DIR}/core.py
                  ${CMAKE_CURRENT_BINARY_DIR}/core.py

              COMMAND ${CMAKE_COMMAND} -E copy
                  ${CMAKE_CURRENT_SOURCE_DIR}/roimanager.py
                  ${CMAKE_CURRENT_BINARY_DIR}/roimanager.py

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
      ${Charm_DIR}/Charm.System.cpp
      ${repository}/version.cpp
    )



    if(ENTANGLE_ROOT )
     if(WIN32)
      message("INSTALL_DIR = ${INSTALL_DIR}")

     install(
        TARGETS ${PROJECT_NAME}
          RUNTIME DESTINATION ${ENTANGLE_ROOT}/device/charming
          LIBRARY DESTINATION ${ENTANGLE_ROOT}/device/charming
     )
     endif()
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

     list(APPEND ENTANGLE_FILES_DEVICE_CHARMING
      ${CMAKE_CURRENT_BINARY_DIR}/${tmpPREFIX}${PROJECT_NAME}${tmpSUFFIX}
      )
    endif()

endforeach()


list(APPEND ENTANGLE_FILES_DEVICE_CHARMING
   ${CMAKE_CURRENT_SOURCE_DIR}/charm.py
   ${CMAKE_CURRENT_SOURCE_DIR}/mesytec.py
   ${CMAKE_CURRENT_SOURCE_DIR}/mesytec-replay.py
   ${CMAKE_CURRENT_SOURCE_DIR}/msmtsystem.py
   ${CMAKE_CURRENT_SOURCE_DIR}/core.py
   ${CMAKE_CURRENT_SOURCE_DIR}/roimanager.py
   ${CMAKE_CURRENT_SOURCE_DIR}/settings.py
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator.py
   ${CMAKE_CURRENT_SOURCE_DIR}/test-replay.py
   ${CMAKE_CURRENT_SOURCE_DIR}/test.py
)

list (APPEND ENTANGLE_FILES ${ENTANGLE_FILES_DEVICE_CHARMING})

list(APPEND ENTANGLE_FILES_RES
   ${CMAKE_CURRENT_SOURCE_DIR}/entangle/example/listmode_replay.res
    ${CMAKE_CURRENT_SOURCE_DIR}/entangle/example/SANS1_detector.res
    ${CMAKE_CURRENT_SOURCE_DIR}/entangle/example/ERWIN_detector.res
  )





if(ENTANGLE_ROOT )

install(
  FILES
  ${ENTANGLE_FILES_DEVICE_CHARMING}
   DESTINATION ${ENTANGLE_ROOT}/device/charming
  )

  install(
  FILES
  ${ENTANGLE_FILES_RES}
   DESTINATION ${ENTANGLE_RES}
   OPTIONAL
  )


endif()


list(APPEND ENTANGLE_FILES
[/etc/entangle]
)

list(APPEND ENTANGLE_FILES ${ENTANGLE_FILES_RES}
)

set(txt ${ENTANGLE_FILES})
string (REPLACE ";" "\n" txt "${txt}")
message(${txt})
FILE(WRITE "${parentdir}/ENTANGLE_FILES" ${txt})
message(STATUS "written to : ${parentdir}/ENTANGLE_FILES")




add_subdirectory (entangle)


