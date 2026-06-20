# ARM bare-metal toolchain file for the xBuddy DOOM firmware.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_BIN "S:/Prusa Can It Run Doom/toolchain/armgcc/arm-gnu-toolchain-13.2.Rel1-mingw-w64-i686-arm-none-eabi/bin")
set(TC_PREFIX "${TOOLCHAIN_BIN}/arm-none-eabi-")

set(CMAKE_C_COMPILER   "${TC_PREFIX}gcc.exe")
set(CMAKE_CXX_COMPILER "${TC_PREFIX}g++.exe")
set(CMAKE_ASM_COMPILER "${TC_PREFIX}gcc.exe")
set(CMAKE_OBJCOPY      "${TC_PREFIX}objcopy.exe" CACHE INTERNAL "")
set(CMAKE_SIZE         "${TC_PREFIX}size.exe"    CACHE INTERNAL "")

# Don't try to link a test executable during compiler check (no syscalls yet).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
