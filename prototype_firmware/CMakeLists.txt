cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME prototype_firmware)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Build CMSIS-DSP from source (vendored in Drivers/CMSIS/DSP).
# CMSISCORE points at the directory holding CMSIS-Core's Include/ (core_cm7.h).
set(CMSISCORE ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS)
# Cortex-M7 is Armv7E-M: it has the DSP extension and an FPU, but no Helium/MVE
# and no Neon, so leave those vector backends off.
set(HELIUM OFF)
set(MVEI OFF)
set(MVEF OFF)
set(MVEFLOAT16 OFF)
set(NEON OFF)
# Armv7E-M has no half-precision arithmetic, so skip the float16 kernels.
set(DISABLEFLOAT16 ON)
add_subdirectory(Drivers/CMSIS/DSP)

# CMSIS-DSP includes core_cm7.h directly rather than the ST device header, so it
# needs the FPU configuration macros that stm32f767xx.h would otherwise supply.
target_compile_definitions(CMSISDSP PRIVATE
    __FPU_PRESENT=1U
    __DSP_PRESENT=1U
)

file(GLOB_RECURSE TGT_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/BGT60TRxx/Src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/DSP/Src/*.c"
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${TGT_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/radar.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/radar_processing.c"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/BGT60TRxx/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # Add user defined libraries
    CMSISDSP
)

# Emit the .hex and .bin images that flashing tools expect alongside the .elf.
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex and ${CMAKE_PROJECT_NAME}.bin"
)

# The board is a NUCLEO-F767ZI, which carries an on-board ST-LINK.
# Override with e.g. -DFLASH_TOOL=probe-rs at configure time.
set(FLASH_TOOL "openocd" CACHE STRING "Tool used by the 'flash' target")
set_property(CACHE FLASH_TOOL PROPERTY STRINGS openocd st-flash probe-rs)

if(FLASH_TOOL STREQUAL "openocd")
    # Programs the .elf directly; verifies, resets into the new image, exits.
    set(FLASH_COMMAND openocd -f board/st_nucleo_f7.cfg
        -c "program $<TARGET_FILE:${CMAKE_PROJECT_NAME}> verify reset exit")
elseif(FLASH_TOOL STREQUAL "st-flash")
    # Takes a raw image, so it needs the base of flash stated explicitly.
    set(FLASH_COMMAND st-flash --reset write
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin 0x08000000)
elseif(FLASH_TOOL STREQUAL "probe-rs")
    set(FLASH_COMMAND probe-rs download --chip STM32F767ZI
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
else()
    message(FATAL_ERROR
        "Unknown FLASH_TOOL '${FLASH_TOOL}' (expected openocd, st-flash or probe-rs)")
endif()

add_custom_target(flash
    COMMAND ${FLASH_COMMAND}
    USES_TERMINAL
    COMMENT "Flashing ${CMAKE_PROJECT_NAME} via ${FLASH_TOOL}"
)
add_dependencies(flash ${CMAKE_PROJECT_NAME})
