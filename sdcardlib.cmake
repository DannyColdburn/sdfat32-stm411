add_library(
  sdcardlib
  OBJECT
  ${CMAKE_CURRENT_LIST_DIR}/sdcard.c 
  ${CMAKE_CURRENT_LIST_DIR}/fs.c
  ${CMAKE_CURRENT_LIST_DIR}/fsutils.c
  ${CMAKE_CURRENT_LIST_DIR}/cluster.c
  )

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/
  )
