set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
#set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-as)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_READELF arm-none-eabi-readelf)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE_UTIL arm-none-eabi-size)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

cmake_minimum_required(VERSION 3.27)

set(PATH_TO_DEV ~/stm32libs/stm32dev)

#set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft")
#set(CMAKE_CXX_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft")

project(F411-FAT32-Test C ASM)

#file(GLOB source_files CONFIGURE_DEPENDS *.c)
#file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/)
add_executable(${PROJECT_NAME} ${PATH_TO_DEV}/stm32f411_startup.c main.c)
target_link_libraries(F411-FAT32-Test PRIVATE F411-FAT32)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    DEBUG_D
)

target_compile_options(
    ${PROJECT_NAME} PRIVATE 
    -mcpu=cortex-m4 
    -mfloat-abi=hard -march=armv7e-m -mfpu=fpv4-sp-d16
    -mthumb  
    -ffunction-sections 
    -fdata-sections 
    -O0 
    -Wall 
    -pedantic
)


#-mfpu=fpv4-sp-d16

target_include_directories(
    ${PROJECT_NAME} PRIVATE 
    "/home/danny/stm32libs/STM32CubeF4/Drivers/CMSIS/Include" 
    "/home/danny/stm32libs/cmsis_device_f4/Include"
    ${CMAKE_SOURCE_DIR}/inc/
)

target_link_directories(
    ${PROJECT_NAME} PRIVATE
    ""
)

target_link_options(
    ${PROJECT_NAME} PRIVATE 
    -T ${CMAKE_SOURCE_DIR}/dev/linker.ld    
    -nostartfiles
    -mthumb
    -mcpu=cortex-m4
    --specs=nano.specs
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    --specs=nosys.specs    
    #--specs=rdimon.specs
    -Wl,--gc-sections,--print-memory-usage
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ARGS -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
)

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_SIZE_UTIL} ARGS ${CMAKE_PROJECT_NAME}.elf
)

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} ARGS ${CMAKE_PROJECT_NAME}.elf -x -D > ${PROJECT_NAME}.lst
)

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
