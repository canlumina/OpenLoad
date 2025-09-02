#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import serial
import serial.tools.list_ports
import threading
import time
from queue import Queue
from typing import List, Dict, Optional, Callable
import logging

logger = logging.getLogger(__name__)

class SerialDevice:
    """串口设备类"""
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_port: Optional[serial.Serial] = None
        self.is_connected = False
        self.read_thread: Optional[threading.Thread] = None
        self.message_queue = Queue()
        self.callbacks: List[Callable] = []
        self._stop_flag = threading.Event()
        
    def add_callback(self, callback: Callable[[str], None]):
        """添加消息回调函数"""
        self.callbacks.append(callback)
        
    def remove_callback(self, callback: Callable[[str], None]):
        """移除消息回调函数"""
        if callback in self.callbacks:
            self.callbacks.remove(callback)
    
    def connect(self) -> bool:
        """连接串口"""
        try:
            if self.is_connected:
                return True
                
            self.serial_port = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                write_timeout=1
            )
            
            self.is_connected = True
            self._stop_flag.clear()
            
            # 启动读取线程
            self.read_thread = threading.Thread(
                target=self._read_loop,
                daemon=True,
                name=f"SerialReader-{self.port}"
            )
            self.read_thread.start()
            
            logger.info(f"串口 {self.port} 连接成功")
            return True
            
        except Exception as e:
            logger.error(f"串口 {self.port} 连接失败: {e}")
            self.is_connected = False
            return False
    
    def disconnect(self):
        """断开串口"""
        try:
            self.is_connected = False
            self._stop_flag.set()
            
            if self.read_thread and self.read_thread.is_alive():
                self.read_thread.join(timeout=2)
            
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
                
            logger.info(f"串口 {self.port} 已断开")
            
        except Exception as e:
            logger.error(f"断开串口 {self.port} 时出错: {e}")
        finally:
            self.serial_port = None
            self.read_thread = None
    
    def send_command(self, command: str) -> bool:
        """发送命令"""
        try:
            if not self.is_connected or not self.serial_port:
                return False
                
            data = (command + '\r\n').encode('utf-8')
            bytes_written = self.serial_port.write(data)
            
            if bytes_written == len(data):
                logger.debug(f"发送命令: {command}")
                return True
            else:
                logger.warning(f"命令发送不完整: {command}")
                return False
                
        except Exception as e:
            logger.error(f"发送命令失败: {e}")
            return False
    
    def send_raw_data(self, data: bytes) -> bool:
        """发送原始数据"""
        try:
            if not self.is_connected or not self.serial_port:
                return False
                
            bytes_written = self.serial_port.write(data)
            return bytes_written == len(data)
            
        except Exception as e:
            logger.error(f"发送原始数据失败: {e}")
            return False
    
    def get_messages(self) -> List[str]:
        """获取接收到的消息"""
        messages = []
        while not self.message_queue.empty():
            try:
                messages.append(self.message_queue.get_nowait())
            except:
                break
        return messages
    
    def _read_loop(self):
        """串口数据读取循环"""
        while not self._stop_flag.is_set() and self.is_connected:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    data = self.serial_port.readline()
                    if data:
                        try:
                            message = data.decode('utf-8', errors='replace').strip()
                            if message:
                                # 添加到消息队列
                                self.message_queue.put(message)
                                
                                # 调用回调函数
                                for callback in self.callbacks:
                                    try:
                                        callback(message)
                                    except Exception as e:
                                        logger.error(f"回调函数执行失败: {e}")
                                        
                                logger.debug(f"接收到消息: {message}")
                        except UnicodeDecodeError as e:
                            logger.warning(f"消息解码失败: {e}")
                
                time.sleep(0.01)  # 避免CPU占用过高
                
            except Exception as e:
                logger.error(f"串口读取错误: {e}")
                if not self._stop_flag.is_set():
                    time.sleep(0.1)

class SerialManager:
    """串口管理器"""
    
    def __init__(self):
        self.devices: Dict[str, SerialDevice] = {}
        self.logger = logging.getLogger(self.__class__.__name__)
    
    @staticmethod
    def get_available_ports() -> List[Dict[str, str]]:
        """获取可用串口列表"""
        ports = []
        try:
            for port_info in serial.tools.list_ports.comports():
                ports.append({
                    'name': port_info.device,
                    'description': port_info.description,
                    'hwid': port_info.hwid,
                    'manufacturer': getattr(port_info, 'manufacturer', 'Unknown')
                })
        except Exception as e:
            logger.error(f"获取串口列表失败: {e}")
        
        return sorted(ports, key=lambda x: x['name'])
    
    def create_device(self, port: str, baudrate: int = 115200) -> Optional[SerialDevice]:
        """创建串口设备"""
        if port in self.devices:
            return self.devices[port]
        
        device = SerialDevice(port, baudrate)
        self.devices[port] = device
        return device
    
    def remove_device(self, port: str):
        """移除串口设备"""
        if port in self.devices:
            device = self.devices[port]
            if device.is_connected:
                device.disconnect()
            del self.devices[port]
    
    def get_device(self, port: str) -> Optional[SerialDevice]:
        """获取串口设备"""
        return self.devices.get(port)
    
    def get_connected_devices(self) -> List[SerialDevice]:
        """获取已连接的设备"""
        return [device for device in self.devices.values() if device.is_connected]
    
    def disconnect_all(self):
        """断开所有设备"""
        for device in self.devices.values():
            if device.is_connected:
                device.disconnect()
    
    def cleanup(self):
        """清理资源"""
        self.disconnect_all()
        self.devices.clear()

# 全局串口管理器实例
serial_manager = SerialManager()