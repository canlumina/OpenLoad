# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## High-level Architecture

This project is a highly portable universal Bootloader framework for various microcontroller architectures (ARM Cortex-M, RISC-V, AVR, PIC, etc.). The core design is a Hardware Abstraction Layer (HAL) architecture, which allows the same code to be easily adapted to different microcontroller platforms without rewriting the core logic.

Key features include:
- Cross-platform support through HAL.
- Modular architecture.
- Security features like CRC check, AES encryption, and digital signature.
- Multiple communication interfaces (UART, USB, CAN, Ethernet).
- Lightweight and efficient.
- Reliable updates with dual-bank backup and power-off protection.

The system is composed of:
- Bootloader Core
- Hardware Abstraction Layer (HAL)
- Communication Protocol Layer
- Security Module

## Common Commands

The project uses CMake and Make for building. The main build files are located in `example/gcc/build`.

### Building the project

To build the bootloader example:
```bash
cd /home/yangcan/Workspaces/OpenLoad/example/gcc/build
make all
```

The output binary will be `openload.elf`.

### Cleaning the build
To clean the build artifacts:
```bash
cd /home/yangcan/Workspaces/OpenLoad/example/gcc/build
make clean
```

### Regenerating build system
To regenerate the build system using CMake:
```bash
cd /home/yangcan/Workspaces/OpenLoad/example/gcc/build
make rebuild_cache
```

## Development Workflow

Porting to a new platform involves:
1.  **Implementing HAL interfaces**: in the `hal` directory (`hal_init()`, `hal_flash_*()`, `hal_comm_*()`).
2.  **Configuring memory layout**: in `boot_config.h` (`APP_START_ADDR`, `FLASH_PAGE_SIZE`, etc.).
3.  **Selecting a communication protocol**: from the `protocols` directory.
4.  **Customizing security options**.
