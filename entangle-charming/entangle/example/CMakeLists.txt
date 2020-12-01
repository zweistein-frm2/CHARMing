cmake_minimum_required (VERSION 3.11)

cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)
message(STATUS ${_host_name})
if(NOT DEFINED ID)
set(ID ${_host_name})
endif()



configure_file( SANS1-det-ID.res.in SANS1-det-${ID}.res )
configure_file( mesytec-listmode-ID-replay.res.in mesytec-listmode-${ID}-replay.res )
if(ENTANGLE_RES)
install(FILES
 ${CMAKE_CURRENT_BINARY_DIR}/SANS1-det-${ID}.res
 ${CMAKE_CURRENT_BINARY_DIR}/mesytec-listmode-${ID}-replay.res
DESTINATION ${ENTANGLE_RES}
)
endif()