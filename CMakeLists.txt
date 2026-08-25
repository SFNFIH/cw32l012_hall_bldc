cmake_minimum_required(VERSION 3.22)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME cw32l012_hall_bldc)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Libraries/inc
    USER/inc
    Board
    BSP
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    CW32L012x8
    "__WEAK=__attribute__((weak))"
)

# Add all driver library sources (exclude system_cw32l012.c, replaced by our override)
file(GLOB_RECURSE DRIVER_SOURCES "Libraries/src/*.c")
list(REMOVE_ITEM DRIVER_SOURCES "${CMAKE_SOURCE_DIR}/Libraries/src/system_cw32l012.c")

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    startup_cw32l012x8.s
    BSP/BSP_led.c
    BSP/BSP_usart.c
    BSP/BSP_halltim.c
    BSP/BSP_motor.c
    BSP/BSP_key.c
    BSP/BSP_adc.c
    BSP/BSP_hfi.c
    USER/src/main.c
    USER/src/interrupts_cw32l012.c
    USER/src/system_cw32l012_override.c
    ${DRIVER_SOURCES}
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    m
)

# Generate hex and bin files after build
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMENT "Generating hex and bin files"
)

# ---------------------------------------------------------------------------
# OpenOCD Flash / Debug targets
# ---------------------------------------------------------------------------
set(OPENOCD_EXE "E:/STM32_Projects/OpenOCD_AT32/bin/openocd.exe")
set(OPENOCD_SCRIPT_DIR "E:/STM32_Projects/OpenOCD_AT32/scripts")
set(OPENOCD_ELF ${CMAKE_PROJECT_NAME}.elf)

add_custom_target(flash
    COMMAND ${OPENOCD_EXE} -s ${OPENOCD_SCRIPT_DIR}
            -f interface/cmsis-dap.cfg
            -f target/cw32l012.cfg
            -c "init"
            -c "reset halt"
            -c "program ${OPENOCD_ELF} verify"
            -c "reset run"
            -c "exit"
    DEPENDS ${OPENOCD_ELF}
    COMMENT "Flashing ${OPENOCD_ELF} via OpenOCD (CMSIS-DAP)"
)

add_custom_target(flash-stlink
    COMMAND ${OPENOCD_EXE} -s ${OPENOCD_SCRIPT_DIR}
            -f interface/stlink.cfg
            -f target/cw32l012.cfg
            -c "init"
            -c "reset halt"
            -c "program ${OPENOCD_ELF} verify"
            -c "reset run"
            -c "exit"
    DEPENDS ${OPENOCD_ELF}
    COMMENT "Flashing ${OPENOCD_ELF} via OpenOCD (ST-Link)"
)

add_custom_target(reset
    COMMAND ${OPENOCD_EXE} -s ${OPENOCD_SCRIPT_DIR}
            -f interface/cmsis-dap.cfg
            -f target/cw32l012.cfg
            -c "init"
            -c "reset run"
            -c "exit"
    COMMENT "Resetting target via OpenOCD"
)

add_custom_target(erase
    COMMAND ${OPENOCD_EXE} -s ${OPENOCD_SCRIPT_DIR}
            -f interface/cmsis-dap.cfg
            -f target/cw32l012.cfg
            -c "init"
            -c "reset halt"
            -c "flash erase_sector 0 0 last"
            -c "exit"
    COMMENT "Erasing chip flash via OpenOCD"
)

add_custom_target(debug-ocd
    COMMAND ${OPENOCD_EXE} -s ${OPENOCD_SCRIPT_DIR}
            -f interface/cmsis-dap.cfg
            -f target/cw32l012.cfg
    COMMENT "Starting OpenOCD server for GDB debug"
)
