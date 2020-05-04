

cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)
message(**${_host_name}*********)
if(NOT DEFINED ID)
set(ID ${_host_name})
endif()
if(WIN32)
set(Entangle_Res ${ENTANGLE_ROOT}/example)
else()
set(Entangle_Res /etc/entangle)
endif()
cmake_minimum_required (VERSION 3.14)

#project(create_res_files)
message(***************${ID}**************)
configure_file( charm-ID-SANS1.res.in charm-${ID}-SANS1.res )
configure_file( charm-ID-replay.res.in charm-${ID}-replay.res )

install(FILES
 charm-${ID}-SANS1.res
 charm-${ID}-replay.res
DESTINATION ${Entangle_Res}
)
