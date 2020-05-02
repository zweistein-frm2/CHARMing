# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.14)
add_subdirectory (entangle)
set(PROJECT1 mesytecsystem)
set(PROJECT2 listmodereplay)



foreach(PROJECT_NAME IN LISTS PROJECT1 PROJECT2)
    message(STATUS "PROJECT_NAME=${PROJECT_NAME}")



project(${PROJECT_NAME})
add_compile_definitions(ENTANGLE_LOGGER)
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
else()
message("on linux boost >= 1.71 must be installed :  sudo apt-get install libboost-python-dev")

#set(BOOST_ROOT /usr )
#set(Boost_INCLUDE_DIR ${BOOST_ROOT}/include)
endif()
  
add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
set(CMAKE_CXX_STANDARD 17) 

find_package (Python COMPONENTS Interpreter Development NumPy)


#set(Boost_USE_STATIC_LIBS,ON)
#set(Boost_USE_STATIC_RUNTIME,ON)
find_package(Boost 1.71 REQUIRED COMPONENTS python${Python_VERSION_MAJOR}${Python_VERSION_MINOR} numpy${Python_VERSION_MAJOR}${Python_VERSION_MINOR} filesystem chrono thread iostreams) # header only libraries must not be added here
find_path(Boost_INCLUDE_DIR boost)

message(********************Using Boost :${Boost_INCLUDE_DIR}  ******************************)
find_package(OpenCV REQUIRED)
find_package(unofficial-libuv CONFIG REQUIRED)

set (magic_enum_DIR ${_VCPKG_ROOT_DIR}/installed/${VCPKG_TARGET_TRIPLET}/share/magic_enum)
message(******************** ${magic_enum_DIR}   ************)
find_package(magic_enum CONFIG REQUIRED )

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
        COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_SOURCE_DIR}/charm-replay.py
              ${CMAKE_CURRENT_BINARY_DIR}/charm-replay.py
         COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_SOURCE_DIR}/test-replay.py
              ${CMAKE_CURRENT_BINARY_DIR}/test-replay.py
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/charm.py
                ${CMAKE_CURRENT_BINARY_DIR}/charm.py
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/test.py
                ${CMAKE_CURRENT_BINARY_DIR}/test.py
                
              )

target_sources(${PROJECT_NAME}
PRIVATE
${PROJECT_NAME}.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv2_converter.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv3_converter.cpp
${pyboostcvconverter_DIR}/src/pyboost_cv4_converter.cpp
${Charm_DIR}/Entangle.Logger.cpp
${Charm_DIR}/Zweistein.ping.cpp

)
if(MSVC)
set(Entangle_Dir /users/alanghof/source/repos/entangle/entangle)
else()
set(Entangle_Dir /usr/lib/python3/dist-packages/entangle)
endif()


install(
    TARGETS ${PROJECT_NAME}
      RUNTIME DESTINATION ${Entangle_Dir}/device/charm
      LIBRARY DESTINATION ${Entangle_Dir}/device/charm
   
       
)

endforeach()

install(
FILES
${CMAKE_SOURCE_DIR}/charm-replay.py
${CMAKE_SOURCE_DIR}/test-replay.py
${CMAKE_SOURCE_DIR}/charm.py
${CMAKE_SOURCE_DIR}/test.py
  DESTINATION ${Entangle_Dir}/device/charm

)