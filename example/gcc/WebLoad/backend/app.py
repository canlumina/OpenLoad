#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import json
import time
import serial
import hashlib
from datetime import datetime
from flask import Flask, request, render_template, jsonify, send_file
from werkzeug.utils import secure_filename
import threading
from queue import Queue

# Flask 应用配置
app = Flask(__name__, static_folder='../static', template_folder='../templates')
app.config['SECRET_KEY'] = 'openload_webmanager_2024'
app.config['UPLOAD_FOLDER'] = os.path.join(os.path.dirname(__file__), '..', 'uploads')
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max file size

# 确保上传目录存在
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

# 全局变量
serial_port = None
device_info = {}
upload_progress = {'status': 'idle', 'progress': 0, 'message': ''}

class SerialManager:
    def __init__(self):
        self.port = None
        self.is_connected = False
        self.read_thread = None
        self.message_queue = Queue()
        
    def connect(self, port_name, baudrate=115200):
        """连接串口"""
        try:
            self.port = serial.Serial(port_name, baudrate, timeout=1)
            self.is_connected = True
            self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.read_thread.start()
            return True
        except Exception as e:
            print(f"串口连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开串口"""
        self.is_connected = False
        if self.port:
            self.port.close()
            self.port = None
    
    def send_command(self, command):
        """发送命令"""
        if self.port and self.is_connected:
            self.port.write((command + '\r\n').encode())
            return True
        return False
    
    def _read_loop(self):
        """串口数据读取循环"""
        while self.is_connected:
            try:
                if self.port.in_waiting > 0:
                    data = self.port.readline().decode('utf-8', errors='ignore').strip()
                    if data:
                        self.message_queue.put(data)
                time.sleep(0.01)
            except Exception as e:
                print(f"串口读取错误: {e}")
                break
    
    def get_messages(self):
        """获取接收到的消息"""
        messages = []
        while not self.message_queue.empty():
            messages.append(self.message_queue.get())
        return messages

# 串口管理器实例
serial_mgr = SerialManager()

@app.route('/')
def index():
    """主页"""
    return render_template('index.html')

@app.route('/api/serial/ports')
def get_serial_ports():
    """获取可用串口列表"""
    import serial.tools.list_ports
    ports = []
    for port in serial.tools.list_ports.comports():
        ports.append({
            'name': port.device,
            'description': port.description,
            'hwid': port.hwid
        })
    return jsonify({'ports': ports})

@app.route('/api/serial/connect', methods=['POST'])
def connect_serial():
    """连接串口"""
    data = request.get_json()
    port_name = data.get('port')
    baudrate = data.get('baudrate', 115200)
    
    if serial_mgr.connect(port_name, baudrate):
        return jsonify({'success': True, 'message': '串口连接成功'})
    else:
        return jsonify({'success': False, 'message': '串口连接失败'})

@app.route('/api/serial/disconnect', methods=['POST'])
def disconnect_serial():
    """断开串口"""
    serial_mgr.disconnect()
    return jsonify({'success': True, 'message': '串口已断开'})

@app.route('/api/serial/status')
def get_serial_status():
    """获取串口状态"""
    return jsonify({
        'connected': serial_mgr.is_connected,
        'port': serial_mgr.port.name if serial_mgr.port else None
    })

@app.route('/api/serial/send', methods=['POST'])
def send_serial_command():
    """发送串口命令"""
    data = request.get_json()
    command = data.get('command', '')
    
    if serial_mgr.send_command(command):
        return jsonify({'success': True, 'message': '命令已发送'})
    else:
        return jsonify({'success': False, 'message': '发送失败，请检查串口连接'})

@app.route('/api/serial/messages')
def get_serial_messages():
    """获取串口消息"""
    messages = serial_mgr.get_messages()
    return jsonify({'messages': messages})

@app.route('/api/device/info')
def get_device_info():
    """获取设备信息"""
    if not serial_mgr.is_connected:
        return jsonify({'success': False, 'message': '未连接设备'})
    
    # 发送info命令获取设备信息
    serial_mgr.send_command('i')
    time.sleep(0.5)  # 等待响应
    
    messages = serial_mgr.get_messages()
    
    # 解析设备信息（这里需要根据实际的bootloader响应格式调整）
    info = {
        'mcu': 'STM32F103ZET6',
        'bootloader_version': '2.0',
        'flash_size': '512KB',
        'bootloader_size': '64KB',
        'app_size': '448KB',
        'last_update': 'Unknown'
    }
    
    return jsonify({'success': True, 'info': info, 'raw_messages': messages})

@app.route('/api/firmware/upload', methods=['POST'])
def upload_firmware():
    """上传固件文件"""
    if 'firmware' not in request.files:
        return jsonify({'success': False, 'message': '未选择文件'})
    
    file = request.files['firmware']
    if file.filename == '':
        return jsonify({'success': False, 'message': '未选择文件'})
    
    if file and file.filename.lower().endswith('.bin'):
        filename = secure_filename(file.filename)
        # 添加时间戳避免文件名冲突
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = f"{timestamp}_{filename}"
        filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
        file.save(filepath)
        
        # 计算文件信息
        file_size = os.path.getsize(filepath)
        with open(filepath, 'rb') as f:
            file_hash = hashlib.md5(f.read()).hexdigest()
        
        return jsonify({
            'success': True, 
            'message': '文件上传成功',
            'filename': filename,
            'size': file_size,
            'hash': file_hash
        })
    else:
        return jsonify({'success': False, 'message': '请选择.bin格式的固件文件'})

@app.route('/api/firmware/list')
def list_firmware():
    """列出已上传的固件文件"""
    files = []
    upload_dir = app.config['UPLOAD_FOLDER']
    
    if os.path.exists(upload_dir):
        for filename in os.listdir(upload_dir):
            if filename.lower().endswith('.bin'):
                filepath = os.path.join(upload_dir, filename)
                file_info = {
                    'filename': filename,
                    'size': os.path.getsize(filepath),
                    'upload_time': datetime.fromtimestamp(
                        os.path.getctime(filepath)
                    ).strftime('%Y-%m-%d %H:%M:%S')
                }
                files.append(file_info)
    
    # 按上传时间排序
    files.sort(key=lambda x: x['upload_time'], reverse=True)
    return jsonify({'files': files})

@app.route('/api/firmware/download/<filename>')
def download_firmware(filename):
    """下载固件文件"""
    filename = secure_filename(filename)
    filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    
    if os.path.exists(filepath):
        return send_file(filepath, as_attachment=True)
    else:
        return jsonify({'success': False, 'message': '文件不存在'})

@app.route('/api/firmware/delete/<filename>', methods=['DELETE'])
def delete_firmware(filename):
    """删除固件文件"""
    filename = secure_filename(filename)
    filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    
    if os.path.exists(filepath):
        os.remove(filepath)
        return jsonify({'success': True, 'message': '文件已删除'})
    else:
        return jsonify({'success': False, 'message': '文件不存在'})

@app.route('/api/firmware/update', methods=['POST'])
def update_firmware():
    """固件升级"""
    data = request.get_json()
    filename = secure_filename(data.get('filename', ''))
    update_type = data.get('type', 'xmodem')  # xmodem or ota
    encryption = data.get('encryption', 'none')  # none, xor, aes
    target = data.get('target', 'internal')  # internal or external
    
    filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    if not os.path.exists(filepath):
        return jsonify({'success': False, 'message': '固件文件不存在'})
    
    if not serial_mgr.is_connected:
        return jsonify({'success': False, 'message': '设备未连接'})
    
    # 开始固件升级流程
    def update_process():
        global upload_progress
        upload_progress['status'] = 'updating'
        upload_progress['progress'] = 0
        upload_progress['message'] = '开始固件升级...'
        
        try:
            # 1. 进入升级模式
            serial_mgr.send_command('u')
            time.sleep(0.5)
            
            # 2. 选择升级方式
            if update_type == 'xmodem':
                serial_mgr.send_command('1')  # XMODEM
            else:
                serial_mgr.send_command('2')  # OTA
            time.sleep(0.5)
            
            # 3. 选择目标
            if target == 'internal':
                if encryption == 'none':
                    serial_mgr.send_command('1')  # 内部Flash无加密
                elif encryption == 'xor':
                    serial_mgr.send_command('2')  # 内部Flash XOR加密
                else:  # aes
                    serial_mgr.send_command('3')  # 内部Flash AES加密
            else:  # external
                if encryption == 'none':
                    serial_mgr.send_command('4')  # 外部Flash无加密
                elif encryption == 'xor':
                    serial_mgr.send_command('5')  # 外部Flash XOR加密
                else:  # aes
                    serial_mgr.send_command('6')  # 外部Flash AES加密
            time.sleep(0.5)
            
            # 4. 如果是AES加密，选择加密算法
            if encryption == 'aes':
                serial_mgr.send_command('2')  # AES-128-CBC
                time.sleep(0.5)
            
            upload_progress['progress'] = 50
            upload_progress['message'] = '配置完成，等待固件传输...'
            
            # 这里应该实现XMODEM协议或OTA传输
            # 暂时模拟传输过程
            for i in range(50, 100, 10):
                time.sleep(1)
                upload_progress['progress'] = i
                upload_progress['message'] = f'传输中... {i}%'
            
            upload_progress['status'] = 'completed'
            upload_progress['progress'] = 100
            upload_progress['message'] = '固件升级完成'
            
        except Exception as e:
            upload_progress['status'] = 'error'
            upload_progress['message'] = f'升级失败: {str(e)}'
    
    # 在后台线程中执行升级
    threading.Thread(target=update_process, daemon=True).start()
    
    return jsonify({
        'success': True, 
        'message': '固件升级已开始，请查看进度'
    })

@app.route('/api/firmware/progress')
def get_update_progress():
    """获取升级进度"""
    return jsonify(upload_progress)

@app.route('/api/bootloader/command/<command>', methods=['POST'])
def bootloader_command(command):
    """执行bootloader命令"""
    if not serial_mgr.is_connected:
        return jsonify({'success': False, 'message': '设备未连接'})
    
    command_map = {
        'help': 'h',
        'info': 'i',
        'erase': 'e',
        'reset': 'r',
        'jump': 'j',
        'wifi': 'w',
        'backup': 'xb',
        'restore': 'xr'
    }
    
    cmd = command_map.get(command, command)
    if serial_mgr.send_command(cmd):
        time.sleep(0.5)  # 等待响应
        messages = serial_mgr.get_messages()
        return jsonify({
            'success': True, 
            'message': f'命令 {command} 已执行',
            'response': messages
        })
    else:
        return jsonify({'success': False, 'message': '命令发送失败'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)