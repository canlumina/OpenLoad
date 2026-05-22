/*
 * OpenLoad - 总入口头
 *
 * 用户工程只需 #include "openload.h" 即可拿到全部公开接口。
 */
#pragma once

#include "openload/errno.h"
#include "openload/config.h"

#include "openload/ops/sys_ops.h"
#include "openload/ops/io_ops.h"
#include "openload/ops/flash_ops.h"

#include "openload/partition.h"
#include "openload/image.h"
#include "openload/logger.h"
#include "openload/crypto.h"

#include "openload/boot.h"
#include "openload/cli.h"
#include "openload/receiver.h"
#include "openload/updater.h"

/* 协议头按需 include, 不强制纳入主入口避免拉低编译速度 */
/* #include "openload/proto/xmodem.h" */

/** 框架版本号 (字符串). */
#define OPENLOAD_VERSION_STR  "0.1.0-m1"
