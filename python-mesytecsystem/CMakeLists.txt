# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)
project(mesytecsystem)
add_compile_definitions(ENTANGLE_LOGGER)
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
else()
message("on linux boost must be installed :  sudo apt-get install libboost-python-dev")
#set(BOOST_ROOT /usr )
#set(Boost_INCLUDE_DIR ${BOOST_ROOT}/include)
endif()
  
add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
set(CMAKE_CXX_STANDARD 17) 

find_package (Python COMPONENTS Interpreter Development NumPy)

find_package(magic_enum CONFIG REQUIRED )

find_package(Boost 1.71 REQUIRED COMPONENTS python${Python_VERSION_MAJOR}${Python_VERSION_MINOR} numpy${Python_VERSION_MAJOR}${Python_VERSION_MINOR} filesystem chrono thread iostreams) # header only libraries must not be added here
find_path(Boost_INCLUDE_DIR boost)

message(********************Using Boost :${Boost_INCLUDE_DIR}  ******************************)
find_package(OpenCV REQUIRED)
find_package(unofficial-libuv CONFIG REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
include_directories(${ASIO_EXTENSIONS_PATH})
include_directories(${SIGSLOT_PATH})
include_directories(${_Python_INCLUDE_DIR})
include_directories(${Charm_DIR})
include_directories(${PacketSender_DIR})
include_directories(${pyboostcvconverter_DIR}/include)
# Add source to this project's executable.
include_directories(${_Python_NumPy_INCLUDE_DIR})
set(Boost_python_library Boost_PYTHON${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_LIBRARY_RELEASE )
set(Boost_numpy_library Boost_NUMPY${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_LIBRARY_RELEASE )
message(**Boost_python_library********** ${Boost_python_library}*********************)
message(****resolves_to***************** ${${Boost_python_library}}**************************)
message(**Boost_numpy_library********** ${Boost_numpy_library}*********************)
message(****resolves_to***************** ${${Boost_numpy_library}}**************************)


add_library(${PROJECT_NAME} SHARED )


target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum ${Boost_LIBRARIES} ${OpenCV_LIBS}  unofficial::libuv::libuv   )
target_link_libraries(${PROJECT_NAME} PRIVATE ${Python_LIBRARY_RELEASE} ${${Boost_python_library}} ${${Boost_numpy_library}})

if(MSVC)
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
else()
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif()
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
     #   COMMAND ${CMAKE_COMMAND} -E copy
     #         ${CMAKE_SOURCE_DIR}/test.py
     #         ${CMAKE_CURRENT_BINARY_DIR}/test.py
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/charm.py
                ${CMAKE_CURRENT_BINARY_DIR}/charm.py
                
              )

target_sources(${PROJECT_NAME}
PRIVATE
"mesytecsystem.cpp"
${pyboostcvconverter_DIR}/src/pyboost_cv2_converter.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv3_converter.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv4_converter.cpp


)