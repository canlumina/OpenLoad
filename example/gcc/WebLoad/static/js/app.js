// OpenLoad Web管理器 JavaScript

// 全局变量
let isConnected = false;
let terminalDiv;
let updateInterval;
let messageInterval;

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', function() {
    terminalDiv = document.getElementById('terminal');
    initializeEventHandlers();
    refreshSerialPorts();
    refreshFirmwareList();
    startMessagePolling();
});

// 初始化事件处理器
function initializeEventHandlers() {
    // 回车发送命令
    document.getElementById('command-input').addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            sendCommand();
        }
    });
    
    // 文件上传处理
    document.getElementById('firmware-file').addEventListener('change', function(e) {
        if (e.target.files.length > 0) {
            const file = e.target.files[0];
            if (file.name.toLowerCase().endsWith('.bin')) {
                addToTerminal(`已选择文件: ${file.name} (${formatFileSize(file.size)})`, 'info');
            } else {
                showToast('请选择.bin格式的固件文件', 'warning');
                e.target.value = '';
            }
        }
    });
}

// 刷新串口列表
async function refreshSerialPorts() {
    try {
        const response = await fetch('/api/serial/ports');
        const data = await response.json();
        
        const portSelect = document.getElementById('serial-port');
        portSelect.innerHTML = '<option value="">选择串口...</option>';
        
        data.ports.forEach(port => {
            const option = document.createElement('option');
            option.value = port.name;
            option.textContent = `${port.name} - ${port.description}`;
            portSelect.appendChild(option);
        });
        
        if (data.ports.length === 0) {
            addToTerminal('未发现可用串口', 'warning');
        } else {
            addToTerminal(`发现 ${data.ports.length} 个串口`, 'info');
        }
    } catch (error) {
        console.error('获取串口列表失败:', error);
        showToast('获取串口列表失败', 'error');
    }
}

// 连接串口
async function connectSerial() {
    const port = document.getElementById('serial-port').value;
    const baudrate = parseInt(document.getElementById('baudrate').value);
    
    if (!port) {
        showToast('请选择串口', 'warning');
        return;
    }
    
    try {
        const response = await fetch('/api/serial/connect', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ port, baudrate })
        });
        
        const data = await response.json();
        
        if (data.success) {
            isConnected = true;
            updateConnectionStatus(true);
            addToTerminal(`已连接到 ${port} (${baudrate} bps)`, 'info');
            showToast(data.message, 'success');
            
            // 启用/禁用按钮
            document.getElementById('connect-btn').disabled = true;
            document.getElementById('disconnect-btn').disabled = false;
        } else {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('连接失败:', error);
        showToast('连接失败', 'error');
    }
}

// 断开串口
async function disconnectSerial() {
    try {
        const response = await fetch('/api/serial/disconnect', {
            method: 'POST'
        });
        
        const data = await response.json();
        
        isConnected = false;
        updateConnectionStatus(false);
        addToTerminal('串口已断开', 'info');
        showToast(data.message, 'info');
        
        // 启用/禁用按钮
        document.getElementById('connect-btn').disabled = false;
        document.getElementById('disconnect-btn').disabled = true;
    } catch (error) {
        console.error('断开失败:', error);
        showToast('断开失败', 'error');
    }
}

// 更新连接状态显示
function updateConnectionStatus(connected) {
    const statusElement = document.getElementById('connection-status');
    if (connected) {
        statusElement.innerHTML = '<i class="bi bi-circle-fill text-success"></i> 已连接';
    } else {
        statusElement.innerHTML = '<i class="bi bi-circle-fill text-danger"></i> 未连接';
    }
}

// 发送串口命令
async function sendCommand() {
    const input = document.getElementById('command-input');
    const command = input.value.trim();
    
    if (!command) return;
    
    if (!isConnected) {
        showToast('请先连接设备', 'warning');
        return;
    }
    
    try {
        addToTerminal(`> ${command}`, 'sent');
        input.value = '';
        
        const response = await fetch('/api/serial/send', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ command })
        });
        
        const data = await response.json();
        
        if (!data.success) {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('发送命令失败:', error);
        showToast('发送命令失败', 'error');
    }
}

// 发送Bootloader命令
async function sendBootloaderCommand(command) {
    if (!isConnected) {
        showToast('请先连接设备', 'warning');
        return;
    }
    
    try {
        addToTerminal(`执行命令: ${command}`, 'info');
        
        const response = await fetch(`/api/bootloader/command/${command}`, {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showToast(data.message, 'success');
            if (data.response) {
                data.response.forEach(msg => {
                    addToTerminal(msg, 'received');
                });
            }
        } else {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('执行命令失败:', error);
        showToast('执行命令失败', 'error');
    }
}

// 获取设备信息
async function getDeviceInfo() {
    if (!isConnected) {
        showToast('请先连接设备', 'warning');
        return;
    }
    
    try {
        const response = await fetch('/api/device/info');
        const data = await response.json();
        
        if (data.success) {
            const info = data.info;
            document.getElementById('device-mcu').textContent = info.mcu;
            document.getElementById('device-bootloader').textContent = info.bootloader_version;
            document.getElementById('device-flash').textContent = info.flash_size;
            document.getElementById('device-update').textContent = info.last_update;
            
            showToast('设备信息已更新', 'success');
            
            if (data.raw_messages) {
                data.raw_messages.forEach(msg => {
                    addToTerminal(msg, 'received');
                });
            }
        } else {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('获取设备信息失败:', error);
        showToast('获取设备信息失败', 'error');
    }
}

// 上传固件
async function uploadFirmware() {
    const fileInput = document.getElementById('firmware-file');
    const file = fileInput.files[0];
    
    if (!file) {
        showToast('请选择固件文件', 'warning');
        return;
    }
    
    if (!file.name.toLowerCase().endsWith('.bin')) {
        showToast('请选择.bin格式的固件文件', 'warning');
        return;
    }
    
    const formData = new FormData();
    formData.append('firmware', file);
    
    try {
        addToTerminal(`开始上传固件: ${file.name}`, 'info');
        
        const response = await fetch('/api/firmware/upload', {
            method: 'POST',
            body: formData
        });
        
        const data = await response.json();
        
        if (data.success) {
            addToTerminal(`上传完成: ${data.filename} (${formatFileSize(data.size)})`, 'success');
            addToTerminal(`文件哈希: ${data.hash}`, 'info');
            showToast(data.message, 'success');
            
            // 清空文件选择
            fileInput.value = '';
            
            // 刷新固件列表
            refreshFirmwareList();
        } else {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('上传失败:', error);
        showToast('上传失败', 'error');
    }
}

// 刷新固件列表
async function refreshFirmwareList() {
    try {
        const response = await fetch('/api/firmware/list');
        const data = await response.json();
        
        const firmwareSelect = document.getElementById('firmware-select');
        const firmwareList = document.getElementById('firmware-list');
        
        // 更新选择列表
        firmwareSelect.innerHTML = '<option value="">选择固件文件...</option>';
        
        // 更新文件列表表格
        firmwareList.innerHTML = '';
        
        if (data.files.length === 0) {
            firmwareList.innerHTML = '<tr><td colspan="4" class="text-center text-muted">暂无固件文件</td></tr>';
        } else {
            data.files.forEach(file => {
                // 添加到选择列表
                const option = document.createElement('option');
                option.value = file.filename;
                option.textContent = `${file.filename} (${formatFileSize(file.size)})`;
                firmwareSelect.appendChild(option);
                
                // 添加到文件列表表格
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${file.filename}</td>
                    <td>${formatFileSize(file.size)}</td>
                    <td>${file.upload_time}</td>
                    <td>
                        <button class="btn btn-outline-primary btn-sm me-1" onclick="downloadFirmware('${file.filename}')">
                            <i class="bi bi-download"></i>
                        </button>
                        <button class="btn btn-outline-danger btn-sm" onclick="deleteFirmware('${file.filename}')">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                `;
                firmwareList.appendChild(row);
            });
        }
    } catch (error) {
        console.error('获取固件列表失败:', error);
        showToast('获取固件列表失败', 'error');
    }
}

// 下载固件
function downloadFirmware(filename) {
    window.open(`/api/firmware/download/${filename}`, '_blank');
}

// 删除固件
async function deleteFirmware(filename) {
    if (!confirm(`确定要删除固件文件 "${filename}" 吗？`)) {
        return;
    }
    
    try {
        const response = await fetch(`/api/firmware/delete/${filename}`, {
            method: 'DELETE'
        });
        
        const data = await response.json();
        
        if (data.success) {
            showToast(data.message, 'success');
            refreshFirmwareList();
        } else {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('删除失败:', error);
        showToast('删除失败', 'error');
    }
}

// 开始固件升级
async function startFirmwareUpdate() {
    const filename = document.getElementById('firmware-select').value;
    
    if (!filename) {
        showToast('请选择固件文件', 'warning');
        return;
    }
    
    if (!isConnected) {
        showToast('请先连接设备', 'warning');
        return;
    }
    
    // 获取升级配置
    const updateType = document.querySelector('input[name="update-type"]:checked').value;
    const target = document.querySelector('input[name="target"]:checked').value;
    const encryption = document.querySelector('input[name="encryption"]:checked').value;
    
    if (!confirm(`确定要升级固件吗？\n\n文件: ${filename}\n方式: ${updateType}\n目标: ${target}\n加密: ${encryption}`)) {
        return;
    }
    
    try {
        const response = await fetch('/api/firmware/update', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                filename,
                type: updateType,
                target,
                encryption
            })
        });
        
        const data = await response.json();
        
        if (data.success) {
            showToast(data.message, 'success');
            startProgressMonitoring();
        } else {
            showToast(data.message, 'error');
        }
    } catch (error) {
        console.error('升级失败:', error);
        showToast('升级失败', 'error');
    }
}

// 开始进度监控
function startProgressMonitoring() {
    const progressDiv = document.getElementById('update-progress');
    const progressBar = document.getElementById('progress-bar');
    const progressMessage = document.getElementById('progress-message');
    
    progressDiv.style.display = 'block';
    
    updateInterval = setInterval(async () => {
        try {
            const response = await fetch('/api/firmware/progress');
            const data = await response.json();
            
            progressBar.style.width = `${data.progress}%`;
            progressBar.textContent = `${data.progress}%`;
            progressMessage.textContent = data.message;
            
            if (data.status === 'completed') {
                clearInterval(updateInterval);
                progressBar.classList.remove('progress-bar-animated');
                progressBar.classList.add('bg-success');
                showToast('固件升级完成！', 'success');
                addToTerminal('固件升级完成', 'success');
            } else if (data.status === 'error') {
                clearInterval(updateInterval);
                progressBar.classList.remove('progress-bar-animated');
                progressBar.classList.add('bg-danger');
                showToast(data.message, 'error');
                addToTerminal(`升级失败: ${data.message}`, 'error');
            }
        } catch (error) {
            console.error('获取进度失败:', error);
            clearInterval(updateInterval);
        }
    }, 1000);
}

// 开始消息轮询
function startMessagePolling() {
    messageInterval = setInterval(async () => {
        if (isConnected) {
            try {
                const response = await fetch('/api/serial/messages');
                const data = await response.json();
                
                data.messages.forEach(message => {
                    addToTerminal(message, 'received');
                });
            } catch (error) {
                console.error('获取消息失败:', error);
            }
        }
    }, 500);
}

// 添加终端消息
function addToTerminal(message, type = 'normal') {
    const timestamp = new Date().toLocaleTimeString();
    const div = document.createElement('div');
    
    let className = '';
    let icon = '';
    
    switch (type) {
        case 'sent':
            className = 'terminal-sent';
            icon = '→ ';
            break;
        case 'received':
            className = 'terminal-received';
            icon = '← ';
            break;
        case 'error':
            className = 'terminal-error';
            icon = '✗ ';
            break;
        case 'success':
            className = 'terminal-success';
            icon = '✓ ';
            break;
        case 'info':
            className = 'terminal-info';
            icon = 'ⓘ ';
            break;
        case 'warning':
            className = 'terminal-warning';
            icon = '⚠ ';
            break;
    }
    
    div.className = className;
    div.innerHTML = `<span class="terminal-timestamp">[${timestamp}]</span> ${icon}${message}`;
    
    terminalDiv.appendChild(div);
    
    // 自动滚动
    if (document.getElementById('auto-scroll').checked) {
        terminalDiv.scrollTop = terminalDiv.scrollHeight;
    }
}

// 清除终端
function clearTerminal() {
    terminalDiv.innerHTML = '';
    addToTerminal('终端已清除', 'info');
}

// 显示Toast通知
function showToast(message, type = 'info') {
    const toast = document.getElementById('toast');
    const toastBody = document.getElementById('toast-body');
    
    // 清除之前的类型类
    toast.classList.remove('toast-success', 'toast-error', 'toast-warning', 'toast-info');
    toast.classList.add(`toast-${type}`);
    
    toastBody.textContent = message;
    
    const bsToast = new bootstrap.Toast(toast);
    bsToast.show();
}

// 格式化文件大小
function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// 页面卸载时清理
window.addEventListener('beforeunload', function() {
    if (updateInterval) clearInterval(updateInterval);
    if (messageInterval) clearInterval(messageInterval);
});