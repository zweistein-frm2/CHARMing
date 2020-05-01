# CMakeList.txt : CMake project for BMesy, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.16)
project(charm)

 add_compile_definitions(SIMPLE_LOGGER)
if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601)
     add_compile_definitions(WIN32_PRE10OLDSTYLE)
endif()
  
add_compile_definitions(ASIOEXT_USE_BOOST_ASIO)
set(CMAKE_CXX_STANDARD 17) 
#find_package (Python COMPONENTS Interpreter Development NumPy)
find_package(magic_enum CONFIG REQUIRED )
find_package(Boost 1.71 REQUIRED COMPONENTS filesystem chrono thread program_options iostreams log log_setup) # header only libraries must not be added here
#find_path(Boost_INCLUDE_DIR boost)
find_package(OpenCV REQUIRED)
find_package(unofficial-libuv CONFIG REQUIRED)
include_directories(${Boost_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
include_directories(${ASIO_EXTENSIONS_PATH})
include_directories(${PacketSender_DIR})
include_directories(./)
include_directories(${SIGSLOT_PATH})
#include_directories(${_Python_INCLUDE_DIR})
# Add source to this project's executable.
add_executable (${PROJECT_NAME} "mesyDeviceCtrl.cpp"  "OptionPrinter.cpp" "CustomOptionDescription.cpp" "simpleLogger.cpp" "coloured_console_sink.cpp" "Zweistein.ping.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE magic_enum::magic_enum ${Boost_LIBRARIES} ${OpenCV_LIBS} unofficial::libuv::libuv )
target_link_options(${PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:GNU>:$<$<CONFIG:RELEASE>:-static-libstdc++> $<$<CONFIG:DEBUG>:-static-libstdc++> > PRIVATE $<$<CXX_COMPILER_ID:GNU>:-static-libgcc>  )

install(
    TARGETS ${PROJECT_NAME}
       
)