cmake_minimum_required(VERSION 3.21)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dev.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/dev.cmake)
    set(CMAKE_TOOLCHAIN_FILE ${TOOLCHAIN_PATH})
    message(STATUS "Happy developing, developer)")
  endif()
endif()

set(C_EXTENSIONS 1)

project(sdcardlib
  VERSION 0.1.0
  DESCRIPTION "SDCard library with tools for FAT32 filesystem. Author: Danny Coldburn"
  LANGUAGES C
  )

include(${LIBS_DIR}/dlutils/dlutils.cmake)

add_library(
  sdcardlib
  OBJECT
  sdcard.h
  sdcard.c
)

target_link_libraries(
  sdcardlib
  dlutils
  )

target_include_directories(${PROJECT_NAME} 
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/
  )

