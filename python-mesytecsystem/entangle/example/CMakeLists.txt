

cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)
message(**${_host_name}*********)
if(NOT DEFINED ID)
set(ID ${_host_name})
endif()

cmake_minimum_required (VERSION 3.14)

#project(create_res_files)
message(***************${ID}**************)
configure_file( charm-ID-SANS1.res.in charm-${ID}-SANS1.res )
configure_file( charm-ID-replay.res.in charm-${ID}-replay.res )
if(ENTANGLE_RES)
install(FILES
 ${CMAKE_CURRENT_BINARY_DIR}/charm-${ID}-SANS1.res
 ${CMAKE_CURRENT_BINARY_DIR}/charm-${ID}-replay.res
DESTINATION ${ENTANGLE_RES}
)
endif()