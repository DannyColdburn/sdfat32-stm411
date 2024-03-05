add_library(
  sdcardlib
  OBJECT
  ${CMAKE_CURRENT_LIST_DIR}/sdcard.c 
  ${CMAKE_CURRENT_LIST_DIR}/fsutils.c
  )

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/
  )
