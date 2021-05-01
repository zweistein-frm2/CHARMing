cmake_minimum_required (VERSION 3.11)

cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)
message(STATUS ${_host_name})
if(NOT DEFINED ID)
set(ID ${_host_name})
endif()



configure_file( SANS1_detector_ID.res.in SANS1_detector_${ID}.res )
configure_file( listmode_ID_replay.res.in listmode_${ID}_replay.res )
configure_file( ERWIN_detector_ID.res.in ERWIN_detector_${ID}.res )
if(ENTANGLE_RES)
install(FILES
 ${CMAKE_CURRENT_BINARY_DIR}/SANS1_detector_${ID}.res
 ${CMAKE_CURRENT_BINARY_DIR}/listmode_${ID}_replay.res
 ${CMAKE_CURRENT_BINARY_DIR}/ERWIN_detector_${ID}.res
DESTINATION ${ENTANGLE_RES}
)
endif()