#!/usr/bin/env python3
"""
OpenLoad image_tool — 给裸 bin 加 OpenLoad 固件头.

输入: 应用程序的原始 .bin (App 工程编译产物, 不含任何头)
输出: <name>-ol.bin = [64-byte header][payload]

固件头格式与 openload/include/openload/image.h 中的 ol_image_header_t 一致.

可选 AES-128-CTR 加密 (M3-4):
    image_tool.py app.bin --aes-key 4F70656E4C6F6164206465...
  - payload 被加密
  - hdr.flags |= OL_IMG_F_ENCRYPTED
  - hdr.aes_iv = 随机 16 字节 (或 --aes-iv 指定)
  - hdr.firmware_crc32 仍是 *明文* CRC (设备解密后比对)
  - 需 pip install pycryptodome
"""

import argparse
import binascii
import os
import struct
import sys
import time
from pathlib import Path

MAGIC          = 0x4F4C4F41   # "AOLO" little-endian
HDR_SIZE       = 64
HDR_FMT_VER    = 1

# 与 image.h 一致
FLAG_ENCRYPTED = 1 << 0
FLAG_SIGNED    = 1 << 1

# struct ol_image_header_t (packed, little-endian) 见 image.h
_HDR_STRUCT = "<I B B H I I I I 16s 16s 4s I"
assert struct.calcsize(_HDR_STRUCT) == HDR_SIZE


def parse_version(s: str) -> int:
    parts = [int(x) for x in s.split(".")]
    while len(parts) < 4:
        parts.append(0)
    if len(parts) > 4 or any(p < 0 or p > 255 for p in parts):
        raise argparse.ArgumentTypeError(f"version must be M.m.p.b, each 0..255")
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]


def parse_hex_bytes(s: str, expect_len: int) -> bytes:
    s = s.strip().replace(" ", "").replace(":", "")
    if len(s) != expect_len * 2:
        raise argparse.ArgumentTypeError(
            f"expect {expect_len}-byte hex ({expect_len*2} chars), got {len(s)}")
    try:
        return bytes.fromhex(s)
    except ValueError as e:
        raise argparse.ArgumentTypeError(f"invalid hex: {e}")


def aes_ctr_encrypt(plaintext: bytes, key: bytes, iv: bytes) -> bytes:
    """AES-128-CTR. IV 当作 128-bit big-endian counter (与 tiny-AES-c 一致)."""
    try:
        from Crypto.Cipher import AES
        from Crypto.Util import Counter
    except ImportError:
        sys.exit("--aes-key requires pycryptodome: pip install pycryptodome")
    ctr = Counter.new(128, initial_value=int.from_bytes(iv, "big"))
    cipher = AES.new(key, AES.MODE_CTR, counter=ctr)
    return cipher.encrypt(plaintext)


def main():
    ap = argparse.ArgumentParser(description="Stamp an OpenLoad image header onto a raw bin.")
    ap.add_argument("input",  type=Path, help="原始 App bin")
    ap.add_argument("-o", "--output", type=Path,
                    help="输出文件 (默认: <input>-ol.bin)")
    ap.add_argument("--board-id",  type=lambda x: int(x, 0), default=0x0103,
                    help="板子 ID (例: 0x0103); 0 = 跨板通用")
    ap.add_argument("--version",   type=parse_version, default=parse_version("0.1.0.0"),
                    help="固件版本 M.m.p.b (默认 0.1.0.0)")
    ap.add_argument("--timestamp", type=int, default=int(time.time()),
                    help="构建 unix 时间戳 (默认: 当前时间)")
    ap.add_argument("--aes-key",   type=lambda x: parse_hex_bytes(x, 16),
                    help="AES-128 key (32 hex 字符); 启用 image 加密")
    ap.add_argument("--aes-iv",    type=lambda x: parse_hex_bytes(x, 16), default=None,
                    help="AES-CTR IV (32 hex 字符); 默认随机生成")
    args = ap.parse_args()

    payload = args.input.read_bytes()
    if len(payload) == 0:
        sys.exit("input file is empty")

    out_path = args.output or args.input.with_name(args.input.stem + "-ol.bin")

    # firmware_crc32 始终是明文 CRC (设备解密后用这个值校验)
    fw_crc = binascii.crc32(payload) & 0xFFFFFFFF

    flags = 0
    iv = b"\x00" * 16
    if args.aes_key is not None:
        flags |= FLAG_ENCRYPTED
        iv = args.aes_iv if args.aes_iv is not None else os.urandom(16)
        payload = aes_ctr_encrypt(payload, args.aes_key, iv)

    # 先组装 header (hdr_crc32 填 0), 算 CRC, 再回填
    hdr = struct.pack(
        _HDR_STRUCT,
        MAGIC,
        HDR_FMT_VER,
        flags,
        args.board_id,
        len(payload),                # = 明文长度 == 密文长度 (CTR stream)
        fw_crc,
        args.version,
        args.timestamp,
        b"\x00" * 16,                # sha256
        iv,                          # aes_iv
        b"\x00" * 4,                 # reserved
        0,                           # hdr_crc32 placeholder
    )
    hdr_crc = binascii.crc32(hdr[: HDR_SIZE - 4]) & 0xFFFFFFFF
    hdr = hdr[: HDR_SIZE - 4] + struct.pack("<I", hdr_crc)

    out_path.write_bytes(hdr + payload)

    print(f"input    : {args.input}  ({len(payload)} bytes)")
    print(f"output   : {out_path}    ({len(payload) + HDR_SIZE} bytes)")
    print(f"board_id : 0x{args.board_id:04x}")
    print(f"version  : {(args.version >> 24) & 0xFF}.{(args.version >> 16) & 0xFF}."
          f"{(args.version >> 8) & 0xFF}.{args.version & 0xFF}")
    print(f"flags    : 0x{flags:02x}{' (encrypted)' if flags & FLAG_ENCRYPTED else ''}")
    if flags & FLAG_ENCRYPTED:
        print(f"aes_iv   : {iv.hex()}")
    print(f"fw_crc32 : 0x{fw_crc:08x} (plaintext)")
    print(f"hdr_crc32: 0x{hdr_crc:08x}")


if __name__ == "__main__":
    main()
