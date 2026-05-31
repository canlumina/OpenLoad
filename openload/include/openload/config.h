/*
 * OpenLoad - 配置入口
 *
 * 编译期配置由 Kconfig 唯一管理:
 *   1. tools/menuconfig.py 编辑 .config
 *   2. CMake 在 configure 阶段调 tools/genconfig.py, 从 .config 生成
 *      openload_autoconfig.h (全量 #define OPENLOAD_*)
 *   3. 用户/示例工程提供的 openload_config.h (shim) #include 它
 *   4. 本文件 include 该 shim, 拿到全部配置宏
 *
 * 不再有 header 兜底默认值 —— Kconfig 是唯一真值来源。未注入配置时下方
 * 哨兵直接报错, 而非静默用上错误的默认值。
 */
#pragma once

/* __has_include 探测用户/示例工程提供的 openload_config.h.
 * ARM GCC 5+ 完全支持; Keil AC6 (基于 clang) 也支持. */
#if defined(__has_include)
#  if __has_include("openload_config.h")
#    include "openload_config.h"
#  endif
#endif

/* 哨兵: OPENLOAD_BOARD_ID 是 Identity 段无 depends 的基础项, 任一 .config 必生成。
 * 它缺失 = openload_autoconfig.h 没被注入 (没跑 Kconfig/genconfig, 或 include
 * 路径没指到生成目录). 直接报错, 不静默兜底. */
#if !defined(OPENLOAD_BOARD_ID)
#  error "OpenLoad: 配置未注入。用 tools/menuconfig.py 编辑 .config, 经 CMake/genconfig 生成 openload_autoconfig.h (见 docs/menuconfig)。"
#endif
