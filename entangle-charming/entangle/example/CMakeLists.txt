cmake_minimum_required (VERSION 3.11)

cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)
message(STATUS ${_host_name})
if(NOT DEFINED ID)
set(ID ${_host_name})
endif()



configure_file( SANS1-detector-ID.res.in SANS1-detector-${ID}.res )
configure_file( mesytec-listmode-ID-replay.res.in mesytec-listmode-${ID}-replay.res )
configure_file( ERWIN-detector-small-ID.res.in ERWIN-detector-small-${ID}.res )
if(ENTANGLE_RES)
install(FILES
 ${CMAKE_CURRENT_BINARY_DIR}/SANS1-detector-${ID}.res
 ${CMAKE_CURRENT_BINARY_DIR}/mesytec-listmode-${ID}-replay.res
 ${CMAKE_CURRENT_BINARY_DIR}/ERWIN-detector-small-${ID}.res
DESTINATION ${ENTANGLE_RES}
)
endif()