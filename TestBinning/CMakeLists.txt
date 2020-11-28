# CMakeList.txt : CMake project for TestBinning, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.16)

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

