## OpenLoad GCC 平台构建指南

本目录提供基于 ARM GCC 的跨平台构建入口，与你的 Keil 平台工程相互独立。

### 先决条件
- ARM GCC 工具链：`arm-none-eabi-gcc`/`arm-none-eabi-objcopy` 等
- CMake 3.20+
- 构建工具：Make 或 Ninja（二选一）

### 目录结构
- `CMakeLists.txt`：GCC 平台的 CMake 入口
- `cmake/arm-gcc-toolchain.cmake`：ARM GCC 工具链定位脚本
- `linker/stm32f103xe.ld`：链接脚本（STM32F103xE，Flash 512KB/RAM 64KB）
- `User/`、`Drivers/`：示例源码（与 keil 平台一致的副本）

### 在本目录下编译
1) 使用 Make（默认）
```bash
cd example/gcc
mkdir -p build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-gcc-toolchain.cmake \
  -DARM_GCC_PREFIX=arm-none-eabi \
  -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

2) 使用 Ninja（若已安装）
```bash
cd example/gcc
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-gcc-toolchain.cmake \
  -DARM_GCC_PREFIX=arm-none-eabi \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 产物
- `build/openload.elf`
- `build/openload.hex`
- `build/openload.bin`
- `build/openload.map`

### 可配置项
- `ARM_GCC_PREFIX`：交叉编译器前缀（默认 `arm-none-eabi`）
- `MCU_MODEL`：MCU 宏（默认 `STM32F103xE`）
- 优化级别：可在 `CMakeLists.txt` 的 `COMMON_C_FLAGS` 增加 `-Os`/`-O2`

### 烧录示例
- ST-LINK
```bash
st-flash --reset write build/openload.bin 0x08000000
```
- OpenOCD
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/openload.elf verify reset exit"
```

### 常见警告与说明
- newlib nano 系统调用未实现（`_write/_read/_close` 等）：由于 `-specs=nosys.specs`，不影响运行。如需 `printf` 输出到串口，可添加简单的 `syscalls.c` 重定向到 `USART1`。
- `openload.elf has a LOAD segment with RWX permissions`：对裸机镜像无功能影响；如需抑制，可在链接选项加入 `-Wl,--no-warn-rwx-segments`。

### 清理
```bash
cd example/gcc
rm -rf build
```
