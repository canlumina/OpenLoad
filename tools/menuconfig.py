#!/usr/bin/env python3
"""OpenLoad menuconfig — kconfiglib ncurses TUI wrapper.

用法:
    python tools/menuconfig.py <path/to/.config>

示例:
    python tools/menuconfig.py examples/stm32f103zet6_gcc/.config
    python tools/menuconfig.py examples/stm32f407vgt6_gcc/.config
"""
import os
import sys
from pathlib import Path

if len(sys.argv) < 2:
    print("usage: python tools/menuconfig.py <path/to/.config>")
    print("  e.g.: python tools/menuconfig.py examples/stm32f103zet6_gcc/.config")
    sys.exit(2)

# kconfiglib 读 srctree 环境变量定位 Kconfig 文件
# (Kconfig 在 repo 根, 所以 srctree = 本脚本的父目录的父目录)
repo_root = Path(__file__).parent.parent
os.environ.setdefault('srctree', str(repo_root))

# KCONFIG_CONFIG 指定 .config 文件路径 (kconfiglib 约定)
os.environ['KCONFIG_CONFIG'] = sys.argv[1]

# 移除脚本目录以防遮挡 kconfiglib 的 menuconfig 模块
_script_dir = str(Path(__file__).parent)
if _script_dir in sys.path:
    sys.path.remove(_script_dir)

# 拉起 kconfiglib 自带的 ncurses menuconfig
from menuconfig import menuconfig
from kconfiglib import Kconfig

kconf = Kconfig(str(repo_root / 'Kconfig'))
menuconfig(kconf)
