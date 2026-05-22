/*
 * OpenLoad - 配置入口
 *
 * 编译期配置流程:
 *   1. 用户工程在 include 路径下提供 openload_config.h (路径任意, 通常工程根)
 *   2. 本文件首先尝试 include 它
 *   3. 然后 include config_default.h 补齐所有缺省项
 *
 * 这意味着用户配置文件可以只写关心的项, 默认值由框架兜底。
 */
#pragma once

/* C++17/GCC __has_include 用于让默认值在用户未提供配置时也能工作.
 * ARM GCC 5+ 完全支持; Keil AC6 (基于 clang) 也支持. */
#if defined(__has_include)
#  if __has_include("openload_config.h")
#    include "openload_config.h"
#  endif
#endif

#include "openload/config_default.h"
