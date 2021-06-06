# CMakeList.txt : CMake project for Boost_Installer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)

project ("entangle-install-charming")
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

get_filename_component(parentdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY )

if(EXISTS "${parentdir}/ENTANGLE_FILES")

set(CMAKE_CXX_STANDARD 17)

file(STRINGS "${parentdir}/ENTANGLE_FILES" ENTANGLE_FILES_RC)

set(RESOURCE_FILES "")
list(LENGTH ENTANGLE_FILES_RC  LL)
message(LIST_LENGTH="${LL}")

FOREACH(FPATH ${ENTANGLE_FILES_RC})
 message(${FPATH})
 string(REGEX MATCH  "[\\[]" isdestdir "${FPATH}")
 #message(isdestdir="${isdestdir}")
 if("${isdestdir}"  STREQUAL "[")
   #message("is a destdirectory")
 else()
 get_filename_component(barename ${FPATH} NAME )
 if(EXISTS ${FPATH})

 else()
 MESSAGE(STATUS "NOT FOUND " ${FPATH})
 set(NOT_ALL_FILES_FOUND True)
 endif()
 if(DEFINED NOT_ALL_FILES_FOUND)
 else()
 if(${barename} MATCHES ".so$")
  MESSAGE("gzip ${barename}")
  execute_process(COMMAND "bash" "-c" "gzip -c  ${FPATH} > ${FPATH}.gz"
  WORKING_DIRECTORY ${parentdir}
                )

  file(COPY ${FPATH}.gz  DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/rcfiles)
  list(APPEND RESOURCE_FILES rcfiles/${barename}.gz)
  else()
  file(COPY ${FPATH}  DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/rcfiles)
  list(APPEND RESOURCE_FILES rcfiles/${barename})
  endif()
 endif()

 endif()

ENDFOREACH()

endif()

if(NOT DEFINED NOT_ALL_FILES_FOUND)

message(RESOURCE_FILES=${RESOURCE_FILES})

include(cmrc/CMakeRC.cmake)
cmrc_add_resource_library(installer-charming ALIAS installer::rc NAMESPACE resources ${RESOURCE_FILES})

add_subdirectory(zlib)
include_directories(zlib)
#message(${CMAKE_CURRENT_BINARY_DIR}/zlib)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/zlib)
include_directories(gzip-hpp/include)

set(Boost_USE_STATIC_LIBS ON)


find_package(Boost ${boost_currentversion} REQUIRED COMPONENTS filesystem chrono thread iostreams ${BOOST_PATHS}) # header only libraries must not be added here
include_directories("${Boost_INCLUDE_DIRS}" )
include_directories(${Charm_DIR})
set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
#include_directories(${Charm_DIR}) # need this only for Zweistein.HomePath.hpp
# Add source to this project's executable.
add_executable (${PROJECT_NAME} "entangle-install-charming.cpp" )

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)

target_link_libraries(${PROJECT_NAME} PRIVATE installer::rc)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries( ${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT} )

target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>)
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>)
target_link_libraries(${PROJECT_NAME} PRIVATE  zlibstatic )
# TODO: Add tests and install targets if needed.


#add_custom_command(TARGET ${PROJECT_NAME}
  #                 POST_BUILD
   #                COMMAND make package
    #              )



install(TARGETS ${PROJECT_NAME}
    DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION  "${INSTALL_DIR}"
    )


else()
 message(WARNING "${parentdir}/ENTANGLE_FILES does not exist or file(s) referered in it do not exist\n"
   "please build entangle-charming to generate ENTANGLE_FILES\n"
   "${PROJECT_NAME} not build.")

endif()
