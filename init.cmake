
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/REPOSITORY_INIT_OK")
   message("REPOSITORY_INIT_OK found, assuming repository already initialised.")
else()
message("init repository ... takes time!")
 if(WIN32)
   execute_process(COMMAND initrepository.bat
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  )

 else()
  execute_process(COMMAND  chmod +x initrepository.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  )
  execute_process(COMMAND ./initrepository.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  )

  endif()


endif()