# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)
project(numpycpp)


  #add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
 
set(CMAKE_CXX_STANDARD 17) 

find_package (Python COMPONENTS Interpreter Development NumPy)

find_package(magic_enum CONFIG REQUIRED )
set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_STATIC_RUNTIME ON)  
find_package(Boost 1.71 REQUIRED COMPONENTS python${Python_VERSION_MAJOR}${Python_VERSION_MINOR} numpy${Python_VERSION_MAJOR}${Python_VERSION_MINOR} filesystem chrono thread iostreams) # header only libraries must not be added here
#find_path(Boost_INCLUDE_DIR boost)
find_package(OpenCV REQUIRED)
#include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
#include_directories(${ASIO_EXTENSIONS_PATH})
include_directories(${SIGSLOT_PATH})
include_directories(${_Python_INCLUDE_DIR})
include_directories(${Charm_DIR})
include_directories(${pyboostcvconverter_DIR}/include)
# Add source to this project's executable.
include_directories(${_Python_NumPy_INCLUDE_DIR})


add_library(${PROJECT_NAME} SHARED )


target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum ${Boost_LIBRARIES} ${OpenCV_LIBS}    )
target_link_libraries(${PROJECT_NAME} PRIVATE ${Python_LIBRARY_RELEASE} $<$<CXX_COMPILER_ID:GNU>:${Boost_PYTHON36_LIBRARY_RELEASE}> $<$<CXX_COMPILER_ID:GNU>:${Boost_NUMPY36_LIBRARY_RELEASE}>)
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>  )

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/test.py
                ${CMAKE_CURRENT_BINARY_DIR}/test.py
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/charm.py
                ${CMAKE_CURRENT_BINARY_DIR}/charm.py
                
              )

target_sources(${PROJECT_NAME}
PRIVATE
${PROJECT_SOURCE_DIR}/NumpyCpp.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv2_converter.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv3_converter.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv4_converter.cpp

)