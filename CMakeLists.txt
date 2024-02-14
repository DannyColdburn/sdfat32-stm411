set(STM_DEVICE STM32F411CEU6)
set(TRIPLET_PREFIX "arm-none-eabi")
set(CMAKE_TOOLCHAIN_FILE /home/danny/stm32libs/toolchain/stm32_toolchain.cmake)

cmake_minimum_required(VERSION 3.27)



project(F411-FAT32 C ASM)

#file(GLOB source_files CONFIGURE_DEPENDS *.c)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_subdirectory(/home/danny/projects/myLibs/dlutils ${CMAKE_CURRENT_BINARY_DIR}/dlutils)

add_executable(${PROJECT_NAME} dev/start.c main.c ${SOURCES})
target_link_libraries(${PROJECT_NAME} dlutils)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    DEBUG_D
)


target_include_directories(
    ${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/inc/
)

generate_binary_files()

#add_custom_target(GenerateBinary ALL DEPENDS ${PROJECT_NAME}.bin)
#add_custom_target(GenerateHex ALL DEPENDS ${PROJECT_NAME}.hex)

#add_custom_command(
#    OUTPUT ${CMAKE_PROJECT_NAME}.bin
#    COMMAND ${CMAKE_OBJCOPY} ARGS -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
#    DEPENDS ${CMAKE_PROJECT_NAME}.elf
#)
#
#add_custom_command(
#    OUTPUT ${CMAKE_PROJECT_NAME}.hex
#    COMMAND ${CMAKE_OBJCOPY} ARGS -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
#    DEPENDS ${CMAKE_PROJECT_NAME}.elf
#)
