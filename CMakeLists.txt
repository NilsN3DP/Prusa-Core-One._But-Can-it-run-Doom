cmake_minimum_required(VERSION 3.20)
project(doom_xbuddy C ASM)

set(CMAKE_C_STANDARD 11)

set(CMSIS_DIR  "${CMAKE_SOURCE_DIR}/../Prusa-Firmware-Buddy/lib/Drivers/CMSIS")
set(GBADOOM    "${CMAKE_SOURCE_DIR}/../GBADoom")

set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16)

# Chain-load by the Prusa bootloader (app @ 0x08020200, packaged as .bbf).
# OFF would produce a raw 0x08000000 image for direct SWD/ST-Link flashing.
option(USE_BOOTLOADER "Build for the Prusa bootloader (.bbf via USB)" ON)
if(USE_BOOTLOADER)
    set(LD_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f427_boot.ld")
    add_compile_definitions(VECT_TAB_BASE=0x08020200u)
else()
    set(LD_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f427.ld")
endif()

add_compile_options(
    ${CPU_FLAGS}
    -ffunction-sections -fdata-sections
    -fno-common
    $<$<CONFIG:Debug>:-Og> $<$<CONFIG:Debug>:-g3>
    $<$<NOT:$<CONFIG:Debug>>:-Os>
    $<$<NOT:$<CONFIG:Debug>>:-flto>
)
add_compile_definitions(STM32F427xx)

set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMSIS_DIR}/Include
    ${CMSIS_DIR}/Device/ST/STM32F4xx/Include
)

# ---- shared low-level drivers ----
set(DRIVERS
    ${CMAKE_SOURCE_DIR}/src/startup_stm32f427.c
    ${CMAKE_SOURCE_DIR}/src/system.c
    ${CMAKE_SOURCE_DIR}/src/syscalls.c
    ${CMAKE_SOURCE_DIR}/src/gpio.c
    ${CMAKE_SOURCE_DIR}/src/spi.c
    ${CMAKE_SOURCE_DIR}/src/ili9488.c
    ${CMAKE_SOURCE_DIR}/src/input.c
    ${CMAKE_SOURCE_DIR}/src/w25q.c
)

set(LINK_FLAGS
    ${CPU_FLAGS}
    -T${LD_SCRIPT}
    -nostartfiles
    --specs=nano.specs
    -flto
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -lc -lm
)

function(add_fw_target name)
    add_executable(${name}.elf ${ARGN})
    target_link_options(${name}.elf PRIVATE ${LINK_FLAGS}
        -Wl,-Map=${CMAKE_BINARY_DIR}/${name}.map)
    add_custom_command(TARGET ${name}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}.elf> ${CMAKE_BINARY_DIR}/${name}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${name}.elf> ${CMAKE_BINARY_DIR}/${name}.hex
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${name}.elf>
        COMMENT "[${name}] bin/hex + size")
endfunction()

# ---- Phase A hardware bring-up ----
add_fw_target(bringup ${DRIVERS} ${CMAKE_SOURCE_DIR}/src/main.c)
target_include_directories(bringup.elf PRIVATE ${COMMON_INCLUDES})

# ---- DOOM engine target ----
file(GLOB ENGINE_C "${GBADOOM}/source/*.c")
list(FILTER ENGINE_C EXCLUDE REGEX "/i_main\\.c$")   # replaced by doom/doom_entry.c
list(FILTER ENGINE_C EXCLUDE REGEX "/i_audio\\.c$")  # GBA (maxmod) audio backend
list(FILTER ENGINE_C EXCLUDE REGEX "/doom_iwad\\.c$") # uses our wad_stub.c instead

set(DOOM_GLUE
    ${CMAKE_SOURCE_DIR}/src/doom/doom_entry.c
    ${CMAKE_SOURCE_DIR}/src/doom/i_backend_stm32.c
    ${CMAKE_SOURCE_DIR}/src/doom/wad_data.c
    ${CMAKE_SOURCE_DIR}/src/doom/sound_stub.c
    ${CMAKE_SOURCE_DIR}/src/doom/tmc_music.c
    ${CMAKE_SOURCE_DIR}/src/doom/fx.c
)

add_fw_target(doom ${DRIVERS} ${DOOM_GLUE} ${ENGINE_C})
target_include_directories(doom.elf PRIVATE ${COMMON_INCLUDES} ${GBADOOM}/include)
target_compile_definitions(doom.elf PRIVATE HAVE_CONFIG_H DOOM_AUTOSTART)
# Engine is gnu11 with many legacy warnings; silence them to keep output readable.
set_source_files_properties(${ENGINE_C} PROPERTIES COMPILE_OPTIONS "-std=gnu11;-w")
# The embedded WAD is a giant const array — compile at -O0 so it lands in flash
# quickly without the optimizer chewing on millions of initializers.
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/doom/wad_data.c
    PROPERTIES COMPILE_OPTIONS "-O0;-w;-fno-lto")
# Newlib calls _read/_write/_sbrk/... from libc (outside the LTO set), so LTO
# would dead-strip them. Keep syscalls.c (and startup) as plain objects.
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/syscalls.c
    PROPERTIES COMPILE_OPTIONS "-fno-lto")
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/startup_stm32f427.c
    PROPERTIES COMPILE_OPTIONS "-fno-lto")
