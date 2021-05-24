# CMakeList.txt : CMake project for TestBinning, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.11)


set(boost_currentversion 1.75.0)

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

if(WIN32)
set(INSTALL_DIR  $ENV{USERPROFILE} ) # or $ENV{HOMEDRIVE}$ENV{HOMEPATH}
else()
set(INSTALL_DIR /usr/local/bin)
endif()


project ("TestBinning")

add_compile_definitions(ENTANGLE_LOGGER)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
    add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

set(CMAKE_CXX_STANDARD 17)
set(Boost_USE_STATIC_LIBS ON)

find_package(Boost ${boost_currentversion} REQUIRED COMPONENTS filesystem chrono thread iostreams log log_setup locale ${BOOST_PATHS}) # header only libraries must not be added here


include_directories(${Charm_DIR})
message(include=${Charm_DIR})

add_compile_options($<$<CXX_COMPILER_ID:GNU>:-Wno-narrowing>)

add_executable (${PROJECT_NAME} "TestBinning.cpp" ${Charm_DIR}/Entangle.Logger.cpp  )

target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>: -Wno-error>)
target_link_libraries(${PROJECT_NAME} PRIVATE   ${Boost_LIBRARIES} )
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>)
target_link_libraries(${PROJECT_NAME} PRIVATE  $<$<CXX_COMPILER_ID:GNU>:-static-libstdc++>)

target_compile_options(${PROJECT_NAME} PRIVATE
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/W3>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/wd4996>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/GS>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/EHsc>>
)

