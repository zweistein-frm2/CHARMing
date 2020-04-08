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
find_package(magic_enum CONFIG REQUIRED )
set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_STATIC_RUNTIME ON)  
find_package(Boost 1.71 REQUIRED COMPONENTS  filesystem chrono thread iostreams log log_setup) # header only libraries must not be added here

include_directories(${Charm_DIR})

add_compile_options($<$<CXX_COMPILER_ID:GNU>:-Wno-narrowing>)

add_executable (TestBinning "TestBinning.cpp" )
target_link_libraries(TestBinning PRIVATE magic_enum::magic_enum ${Boost_LIBRARIES} ) 
target_link_options(TestBinning PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static> )

target_compile_options(TestBinning PRIVATE
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/W3>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/wd4996>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/GS>>
$<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/EHsc>>
)

