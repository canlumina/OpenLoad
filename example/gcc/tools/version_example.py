#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STM32 固件版本管理示例脚本
展示如何使用版本管理功能进行固件开发和部署

使用方法:
python version_example.py [firmware.bin]
"""

import os
import sys
import subprocess
from datetime import datetime

def run_command(cmd):
    """执行命令并返回结果"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, encoding='gbk', errors='ignore')
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)

def increment_version(current_version, increment_type='patch'):
    """自动递增版本号"""
    try:
        parts = current_version.split('.')
        if len(parts) != 4:
            return "1.0.0.1"
        
        major, minor, patch, build = int(parts[0]), int(parts[1]), int(parts[2]), int(parts[3])
        
        # 生成新的构建号（基于时间戳）
        new_build = int(datetime.now().strftime('%y%m%d%H%M'))
        
        if increment_type == 'major':
            major += 1
            minor = patch = 0
        elif increment_type == 'minor':
            minor += 1
            patch = 0
        elif increment_type == 'patch':
            patch += 1
        elif increment_type == 'build':
            pass  # 只更新构建号
        
        return f"{major}.{minor}.{patch}.{new_build}"
    except:
        return "1.0.0.1"

def get_firmware_version(encrypted_file):
    """从加密固件中提取版本信息"""
    print(f"正在分析固件版本信息: {encrypted_file}")
    
    success, output, error = run_command(f"python firmware_decryptor.py \"{encrypted_file}\" temp_decrypt.bin yangcan")
    
    # 清理临时文件
    if os.path.exists("temp_decrypt.bin"):
        os.remove("temp_decrypt.bin")
    
    if success and "固件版本:" in output:
        # 从输出中提取版本号
        for line in output.split('\n'):
            if "固件版本:" in line and "v" in line:
                version_part = line.split("v")[1].split()[0]
                return version_part
    
    return None

def create_versioned_firmware():
    """创建版本化固件的完整示例"""
    
    print("=" * 60)
    print("STM32 固件版本管理示例")
    print("=" * 60)
    
    # 检查输入固件
    if len(sys.argv) > 1:
        input_firmware = sys.argv[1]
    else:
        input_firmware = "RTC2.bin"  # 默认测试固件
    
    if not os.path.exists(input_firmware):
        print(f"错误: 固件文件 {input_firmware} 不存在")
        print("请确保固件文件存在或指定正确的路径")
        return False
    
    print(f"使用固件文件: {input_firmware}")
    
    # 版本管理演示
    scenarios = [
        {"version": "1.0.0.1", "desc": "初始版本"},
        {"version": "1.0.1.2", "desc": "Bug修复版本"},
        {"version": "1.1.0.3", "desc": "功能增强版本"},
        {"version": "2.0.0.4", "desc": "重大更新版本"}
    ]
    
    print(f"\n正在创建不同版本的加密固件...")
    
    for i, scenario in enumerate(scenarios, 1):
        version = scenario["version"]
        desc = scenario["desc"]
        output_file = f"firmware_v{version.replace('.', '_')}.bin"
        
        print(f"\n{i}. 创建 {desc} (v{version})")
        print(f"   输出文件: {output_file}")
        
        # 执行加密
        cmd = f"python firmware_encryptor.py \"{input_firmware}\" \"{output_file}\" yangcan {version}"
        success, output, error = run_command(cmd)
        
        if success:
            print(f"   [OK] 加密成功")
            
            # 验证版本信息
            extracted_version = get_firmware_version(output_file)
            if extracted_version == version:
                print(f"   [OK] 版本验证通过: v{extracted_version}")
            else:
                print(f"   [WARN] 版本验证警告: 期望 v{version}, 实际 v{extracted_version}")
        else:
            print(f"   [ERROR] 加密失败: {error}")
    
    # 版本比较示例
    print(f"\n" + "=" * 60)
    print("版本比较示例")
    print("=" * 60)
    
    versions = ["1.0.0.1", "1.0.1.2", "1.1.0.3", "2.0.0.4"]
    print("版本升级路径:")
    for i in range(len(versions) - 1):
        current = versions[i]
        next_ver = versions[i + 1]
        print(f"  v{current} → v{next_ver}")
    
    # 自动版本递增示例
    print(f"\n" + "=" * 60)
    print("自动版本递增示例")
    print("=" * 60)
    
    base_version = "1.2.3.100"
    print(f"基础版本: v{base_version}")
    
    increment_types = [
        ("patch", "补丁更新"),
        ("minor", "功能更新"),
        ("major", "重大更新"),
        ("build", "重新构建")
    ]
    
    for inc_type, desc in increment_types:
        new_version = increment_version(base_version, inc_type)
        print(f"  {desc}: v{base_version} → v{new_version}")
    
    # 使用建议
    print(f"\n" + "=" * 60)
    print("版本管理最佳实践")
    print("=" * 60)
    
    practices = [
        "1. 使用语义化版本号: major.minor.patch.build",
        "2. Major版本: 重大功能更新或不兼容变更",
        "3. Minor版本: 新功能添加，向后兼容",
        "4. Patch版本: Bug修复和小改动",
        "5. Build版本: 使用时间戳或递增数字",
        "6. 在Git中打标签: git tag v1.2.3.456",
        "7. 升级前检查版本兼容性",
        "8. 保留重要版本的备份"
    ]
    
    for practice in practices:
        print(f"  {practice}")
    
    # Bootloader命令提示
    print(f"\n" + "=" * 60)
    print("Bootloader版本管理命令")
    print("=" * 60)
    
    bootloader_cmds = [
        ("version", "显示当前固件版本信息"),
        ("vcompare", "比较各分区固件版本"),
        ("u", "执行固件升级（支持版本检查）")
    ]
    
    print("连接STM32设备后，可使用以下命令：")
    for cmd, desc in bootloader_cmds:
        print(f"  {cmd:10} - {desc}")
    
    print(f"\n版本管理演示完成！")
    print(f"生成的固件文件可用于XMODEM传输测试。")
    
    return True

if __name__ == "__main__":
    try:
        create_versioned_firmware()
    except KeyboardInterrupt:
        print(f"\n用户中断操作")
    except Exception as e:
        print(f"错误: {str(e)}")