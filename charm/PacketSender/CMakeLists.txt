# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)
project(PacketSender)
set(CMAKE_CXX_STANDARD 17) 

find_package(magic_enum CONFIG REQUIRED )
set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_STATIC_RUNTIME ON)  
find_package(Boost 1.71 REQUIRED COMPONENTS  filesystem chrono thread iostreams) # header only libraries must not be added here
find_path(Boost_INCLUDE_DIR boost)
find_package(unofficial-libuv CONFIG REQUIRED)
include_directories(${Charm_DIR})
include_directories(${Boost_INCLUDE_DIR})
message("boost lib: ${Boost_LIBRARIES}") 
message("boost inc:${Boost_INCLUDE_DIR}") 


add_compile_options($<$<CXX_COMPILER_ID:GNU>:-Wno-narrowing>)
configure_file( cmake_variables.h.in ${CMAKE_BINARY_DIR}/generated/cmake_variables.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ ) # Make sure it can be included...
add_executable (${PROJECT_NAME} "PacketSender.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum ${Boost_LIBRARIES} unofficial::libuv::libuv ) # libstdc++.a libm.a libc.a
target_link_options(${PROJECT_NAME}  PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static> )
# TODO: Add tests and install targets if needed.
