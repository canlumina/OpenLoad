# ----------------------------------------------------------------------------
#  Toolchain - ARM GCC (cross-compile for Cortex-M3)
#
#  工具链前缀解析顺序:
#    1. CMake 变量 OPENLOAD_GCC_PREFIX (cmake -DOPENLOAD_GCC_PREFIX=...)
#    2. 环境变量  OPENLOAD_GCC_PREFIX
#    3. 默认 "arm-none-eabi-" (要求工具链在 PATH 中可见)
# ----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED OPENLOAD_GCC_PREFIX AND DEFINED ENV{OPENLOAD_GCC_PREFIX})
    set(OPENLOAD_GCC_PREFIX $ENV{OPENLOAD_GCC_PREFIX})
endif()
if(NOT DEFINED OPENLOAD_GCC_PREFIX)
    set(OPENLOAD_GCC_PREFIX "arm-none-eabi-")
endif()

set(CMAKE_C_COMPILER   ${OPENLOAD_GCC_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${OPENLOAD_GCC_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${OPENLOAD_GCC_PREFIX}g++)
set(CMAKE_OBJCOPY      ${OPENLOAD_GCC_PREFIX}objcopy)
set(CMAKE_SIZE         ${OPENLOAD_GCC_PREFIX}size)

set(CMAKE_EXECUTABLE_SUFFIX_C   ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ----------------------------------------------------------------------------
#  MCU flags - Cortex-M3, 无 FPU
# ----------------------------------------------------------------------------
set(MCU_FLAGS "-mcpu=cortex-m3 -mthumb")

set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS} \
    -Wall -Wextra \
    -fdata-sections -ffunction-sections \
    -fno-builtin -fshort-enums")

set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp -MMD -MP")

# Debug: 容易调试; Release: 体积优先 (-Os 比 -Oz 稳, 跨工具链兼容性更好).
# 使用 FORCE 避免被 CMake 默认值 (Release=-O3 -DNDEBUG) 覆盖.
set(CMAKE_C_FLAGS_DEBUG   "-Og -g3"          CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-Os -g0 -DNDEBUG" CACHE STRING "" FORCE)

set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_FLAGS} \
    --specs=nano.specs --specs=nosys.specs \
    -Wl,--gc-sections \
    -Wl,--print-memory-usage")

# C 标准: STM32 CubeMX 生成代码要求 C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
