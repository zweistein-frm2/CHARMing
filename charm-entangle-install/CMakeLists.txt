# CMakeList.txt : CMake project for Boost_Installer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)


project ("charm-entangle-install")
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

set(CMAKE_CXX_STANDARD 17)

file(STRINGS "${CMAKE_SOURCE_DIR}/ENTANGLE_FILES" ENTANGLE_FILES_RC)

set(RESOURCE_FILES "")
list(LENGTH ENTANGLE_FILES_RC  LL)
message(LIST_LENGTH="${LL}")
FOREACH(FPATH ${ENTANGLE_FILES_RC})
 get_filename_component(barename ${FPATH} NAME )
  MESSAGE("<<${barename}>>")
  file(COPY ${FPATH}  DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
  list(APPEND RESOURCE_FILES ${barename})
ENDFOREACH()


include(CMakeRC.cmake)
cmrc_add_resource_library(installer-resources ALIAS installer::rc NAMESPACE resources syspath.py ${RESOURCE_FILES})
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread  iostreams  locale ${BOOST_PATHS}) # header only libraries must not be added here
include_directories("${Boost_INCLUDE_DIRS}" )

include_directories(${Charm_DIR})
# Add source to this project's executable.
add_executable (${PROJECT_NAME} "Entangle_Install.cpp" "Entangle_Install.h")

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static> PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>  )

target_link_libraries(${PROJECT_NAME} PRIVATE installer::rc)
target_link_libraries(${PROJECT_NAME} PRIVATE ${Boost_LIBRARIES})
# TODO: Add tests and install targets if needed.


#add_custom_command(TARGET ${PROJECT_NAME}
  #                 POST_BUILD
   #                COMMAND make package
    #              )



install(TARGETS ${PROJECT_NAME}
    DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION  "${INSTALL_DIR}"
    )