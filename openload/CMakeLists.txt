# openload/CMakeLists.txt
#
# 框架核心: 平台无关。由 example 工程通过 add_subdirectory(openload) 引入。
# 输出 STATIC library "openload"。
#
# 协议、加密、命令按配置裁剪——用 cmake -D 传, 或在 example 顶层 CMakeLists 设置:
#   set(OPENLOAD_ENABLE_XMODEM      ON  CACHE BOOL "")
#   set(OPENLOAD_ENABLE_HTTP_OTA    OFF CACHE BOOL "")
#   set(OPENLOAD_ENABLE_AES_128_CTR OFF CACHE BOOL "")
#   set(OPENLOAD_ENABLE_SHA256      OFF CACHE BOOL "")

cmake_minimum_required(VERSION 3.22)

option(OPENLOAD_ENABLE_XMODEM      "Enable XMODEM receiver"      ON)
option(OPENLOAD_ENABLE_YMODEM      "Enable YMODEM receiver"      OFF)
option(OPENLOAD_ENABLE_HTTP_OTA    "Enable HTTP OTA receiver"    OFF)
option(OPENLOAD_ENABLE_OPLOG       "Enable persistent op log"    OFF)
option(OPENLOAD_ENABLE_AES_128_CTR "Enable AES-128-CTR"          OFF)
option(OPENLOAD_ENABLE_SHA256      "Enable SHA-256"              OFF)
option(OPENLOAD_ENABLE_ED25519     "Enable Ed25519 signature"    OFF)

set(OPENLOAD_SOURCES
    core/sys.c
    core/io.c
    core/flash.c
    core/partition.c
    core/image.c
    core/logger.c
    core/cli.c
    core/boot.c
    core/updater.c
    core/ringbuf.c
    core/errno_str.c
    crypto/crc32.c
    commands/builtins.c
    devices/w25qxx.c
)

if(OPENLOAD_ENABLE_XMODEM)
    list(APPEND OPENLOAD_SOURCES proto/xmodem.c)
endif()

if(OPENLOAD_ENABLE_YMODEM)
    list(APPEND OPENLOAD_SOURCES proto/ymodem.c)
endif()

if(OPENLOAD_ENABLE_HTTP_OTA)
    list(APPEND OPENLOAD_SOURCES proto/http_ota.c)
endif()

if(OPENLOAD_ENABLE_OPLOG)
    list(APPEND OPENLOAD_SOURCES core/oplog.c)
endif()

if(OPENLOAD_ENABLE_AES_128_CTR)
    list(APPEND OPENLOAD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tiny-aes/aes.c)
endif()

if(OPENLOAD_ENABLE_SHA256)
    list(APPEND OPENLOAD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sha256/sha256.c)
endif()

if(OPENLOAD_ENABLE_ED25519)
    list(APPEND OPENLOAD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tweetnacl/tweetnacl.c
        crypto/tweetnacl_stub.c)
endif()

add_library(openload STATIC ${OPENLOAD_SOURCES})

target_include_directories(openload PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# tiny-AES: 仅启用 AES-128 + CTR mode, 关掉 ECB/CBC 节省 .text (~1KB).
if(OPENLOAD_ENABLE_AES_128_CTR)
    target_include_directories(openload PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tiny-aes
    )
    target_compile_definitions(openload PRIVATE
        CBC=0
        ECB=0
        CTR=1
        AES128=1
    )
endif()

# Brad Conte SHA-256 (public domain).
if(OPENLOAD_ENABLE_SHA256)
    target_include_directories(openload PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sha256
    )
endif()

# tweetnacl ed25519 (public domain). M4-2.
if(OPENLOAD_ENABLE_ED25519)
    target_include_directories(openload PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tweetnacl
    )
    # vendor code, 一些 sign-compare / unterminated-string 不修, 局部静音
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tweetnacl/tweetnacl.c
        PROPERTIES COMPILE_OPTIONS
        "-Wno-sign-compare;-Wno-unterminated-string-initialization"
    )
endif()

# example 工程的 openload_config.h 必须在 include 路径里 — 由调用方提供
# 通过 target_include_directories(openload PUBLIC <example_dir>) 追加。
