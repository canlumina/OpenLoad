// OpenLoad 后台管理 JavaScript
// 后台页面专用脚本

let currentSection = 'dashboard';

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', function() {
    initializeAdmin();
    refreshDashboard();
    initializeEventHandlers();
});

// 初始化后台管理
function initializeAdmin() {
    // 显示默认部分
    showSection('dashboard');
}

// 初始化事件处理器
function initializeEventHandlers() {
    // API端点选择事件
    const apiSelect = document.getElementById('api-endpoint');
    if (apiSelect) {
        apiSelect.addEventListener('change', function() {
            const customGroup = document.getElementById('custom-endpoint-group');
            if (this.value === 'custom') {
                customGroup.style.display = 'block';
            } else {
                customGroup.style.display = 'none';
            }
        });
    }
}

// 显示指定部分
function showSection(sectionName) {
    // 隐藏所有部分
    const sections = document.querySelectorAll('.section');
    sections.forEach(section => {
        section.style.display = 'none';
    });
    
    // 显示指定部分
    const targetSection = document.getElementById(sectionName);
    if (targetSection) {
        targetSection.style.display = 'block';
    }
    
    // 更新导航状态
    const navLinks = document.querySelectorAll('.sidebar .nav-link');
    navLinks.forEach(link => {
        link.classList.remove('active');
        if (link.getAttribute('href') === `#${sectionName}`) {
            link.classList.add('active');
        }
    });
    
    currentSection = sectionName;
    
    // 根据部分加载相应数据
    if (sectionName === 'firmware-list') {
        refreshFirmwareList();
    }
}

// 刷新仪表板数据
async function refreshDashboard() {
    try {
        // 获取系统统计信息
        const statsResponse = await fetch('/api/v1/system/stats');
        const statsResult = await statsResponse.json();
        
        if (statsResult.success) {
            const stats = statsResult.data;
            
            // 更新统计卡片
            document.getElementById('total-firmwares').textContent = stats.firmwares.total;
            document.getElementById('encrypted-firmwares').textContent = stats.firmwares.encrypted;
            document.getElementById('total-size').textContent = formatFileSize(stats.firmwares.total_size);
        }
        
        // 获取固件列表
        const firmwaresResponse = await fetch('/api/v1/firmwares');
        const firmwaresResult = await firmwaresResponse.json();
        
        if (firmwaresResult.success) {
            const firmwares = firmwaresResult.data.firmwares;
            
            // 更新最新版本
            const latestFirmware = firmwares.find(fw => fw.is_latest);
            document.getElementById('latest-version').textContent = 
                latestFirmware ? latestFirmware.version : '无';
            
            // 更新最近固件列表
            updateRecentFirmwares(firmwares.slice(0, 5));
        }
        
    } catch (error) {
        console.error('刷新仪表板失败:', error);
        showNotification('刷新仪表板失败: ' + error.message, 'error');
    }
}

// 更新最近固件列表
function updateRecentFirmwares(firmwares) {
    const container = document.getElementById('recent-firmwares');
    
    if (firmwares.length === 0) {
        container.innerHTML = '<p class="text-muted">暂无固件</p>';
        return;
    }
    
    let html = '<div class="row">';
    firmwares.forEach(firmware => {
        const uploadTime = new Date(firmware.upload_time).toLocaleString('zh-CN');
        const encryptionBadge = firmware.is_encrypted 
            ? '<span class="badge bg-success">已加密</span>'
            : '<span class="badge bg-secondary">未加密</span>';
        const latestBadge = firmware.is_latest 
            ? '<span class="badge bg-warning text-dark ms-2">最新</span>' 
            : '';
        
        html += `
            <div class="col-md-6 mb-3">
                <div class="card h-100 ${firmware.is_latest ? 'border-warning' : ''}">
                    <div class="card-body">
                        <div class="d-flex justify-content-between align-items-start mb-2">
                            <h6 class="card-title mb-0">${firmware.original_filename}</h6>
                            ${latestBadge}
                        </div>
                        <p class="card-text small text-muted mb-2">
                            版本: <code>${firmware.version}</code><br>
                            大小: ${formatFileSize(firmware.size)}<br>
                            时间: ${uploadTime}
                        </p>
                        <div class="d-flex justify-content-between align-items-center">
                            ${encryptionBadge}
                            <div>
                                <button class="btn btn-sm btn-outline-primary" onclick="downloadFirmware('${firmware.id}')">
                                    <i class="bi bi-download"></i>
                                </button>
                                <button class="btn btn-sm btn-outline-info" onclick="viewFirmwareDetails('${firmware.id}')">
                                    <i class="bi bi-eye"></i>
                                </button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `;
    });
    html += '</div>';
    
    container.innerHTML = html;
}

// 刷新固件列表
async function refreshFirmwareList() {
    const tableBody = document.getElementById('firmware-table-body');
    
    try {
        // 显示加载状态
        tableBody.innerHTML = `
            <tr>
                <td colspan="6" class="text-center py-4">
                    <div class="spinner-border spinner-border-sm me-2"></div>
                    正在加载固件列表...
                </td>
            </tr>
        `;
        
        const response = await fetch('/api/v1/firmwares');
        const result = await response.json();
        
        if (result.success) {
            const firmwares = result.data.firmwares;
            
            if (firmwares.length === 0) {
                tableBody.innerHTML = `
                    <tr>
                        <td colspan="6" class="text-center py-4">
                            <i class="bi bi-inbox text-muted" style="font-size: 2rem;"></i><br>
                            <span class="text-muted">暂无固件文件</span>
                        </td>
                    </tr>
                `;
                return;
            }
            
            let html = '';
            firmwares.forEach(firmware => {
                const uploadTime = new Date(firmware.upload_time).toLocaleString('zh-CN');
                const encryptionStatus = firmware.is_encrypted 
                    ? '<span class="badge bg-success">已加密</span>'
                    : '<span class="badge bg-secondary">未加密</span>';
                const latestBadge = firmware.is_latest 
                    ? '<i class="bi bi-star-fill text-warning me-1" title="最新版本"></i>' 
                    : '';
                
                html += `
                    <tr ${firmware.is_latest ? 'class="table-warning"' : ''}>
                        <td>
                            <div class="d-flex align-items-center">
                                ${latestBadge}
                                <div>
                                    <div class="fw-bold">${firmware.original_filename}</div>
                                    <small class="text-muted">${firmware.filename}</small>
                                </div>
                            </div>
                        </td>
                        <td><code class="user-select-all">${firmware.version}</code></td>
                        <td>${formatFileSize(firmware.size)}</td>
                        <td>${encryptionStatus}</td>
                        <td><small>${uploadTime}</small></td>
                        <td>
                            <div class="btn-group btn-group-sm">
                                <button class="btn btn-outline-primary" onclick="downloadFirmware('${firmware.id}')" title="下载">
                                    <i class="bi bi-download"></i>
                                </button>
                                <button class="btn btn-outline-info" onclick="viewFirmwareDetails('${firmware.id}')" title="详情">
                                    <i class="bi bi-eye"></i>
                                </button>
                                <button class="btn btn-outline-secondary" onclick="copyFirmwareInfo('${firmware.id}')" title="复制信息">
                                    <i class="bi bi-clipboard"></i>
                                </button>
                            </div>
                        </td>
                    </tr>
                `;
            });
            
            tableBody.innerHTML = html;
        } else {
            throw new Error(result.error);
        }
        
    } catch (error) {
        console.error('刷新固件列表失败:', error);
        tableBody.innerHTML = `
            <tr>
                <td colspan="6" class="text-center py-4 text-danger">
                    <i class="bi bi-exclamation-triangle me-2"></i>
                    加载失败: ${error.message}
                </td>
            </tr>
        `;
    }
}

// 测试API
async function testAPI() {
    const endpointSelect = document.getElementById('api-endpoint');
    const customEndpoint = document.getElementById('custom-endpoint');
    const targetDevice = document.getElementById('target-device');
    const statusElement = document.getElementById('test-status');
    const responseElement = document.getElementById('test-response');
    
    let endpoint = endpointSelect.value;
    if (endpoint === 'custom') {
        endpoint = customEndpoint.value.trim();
        if (!endpoint) {
            showNotification('请输入自定义端点', 'warning');
            return;
        }
    }
    
    // 构建查询参数
    const params = new URLSearchParams();
    if (targetDevice.value.trim()) {
        params.append('target_device', targetDevice.value.trim());
    }
    
    const fullUrl = params.toString() ? `${endpoint}?${params}` : endpoint;
    
    try {
        statusElement.textContent = '测试中...';
        statusElement.className = 'text-warning';
        
        const startTime = Date.now();
        const response = await fetch(fullUrl);
        const endTime = Date.now();
        const responseTime = endTime - startTime;
        
        const result = await response.json();
        
        // 显示结果
        const resultInfo = {
            url: fullUrl,
            method: 'GET',
            status: response.status,
            statusText: response.statusText,
            responseTime: `${responseTime}ms`,
            headers: {
                'content-type': response.headers.get('content-type'),
                'content-length': response.headers.get('content-length')
            },
            data: result
        };
        
        responseElement.textContent = JSON.stringify(resultInfo, null, 2);
        
        if (response.ok && result.success) {
            statusElement.textContent = `成功 (${responseTime}ms)`;
            statusElement.className = 'text-success';
        } else {
            statusElement.textContent = `失败 (${response.status})`;
            statusElement.className = 'text-danger';
        }
        
    } catch (error) {
        console.error('API测试失败:', error);
        responseElement.textContent = `错误: ${error.message}`;
        statusElement.textContent = '请求失败';
        statusElement.className = 'text-danger';
    }
}

// 下载固件
function downloadFirmware(firmwareId) {
    window.open(`/api/v1/firmwares/${firmwareId}/download`, '_blank');
}

// 查看固件详情
async function viewFirmwareDetails(firmwareId) {
    try {
        const response = await fetch(`/api/v1/firmwares/${firmwareId}`);
        const result = await response.json();
        
        if (result.success) {
            const firmware = result.data;
            const details = JSON.stringify(firmware, null, 2);
            
            // 使用模态框显示详情
            const modal = new bootstrap.Modal(document.createElement('div'));
            const modalHtml = `
                <div class="modal fade" tabindex="-1">
                    <div class="modal-dialog modal-lg">
                        <div class="modal-content">
                            <div class="modal-header">
                                <h5 class="modal-title">固件详情: ${firmware.original_filename}</h5>
                                <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                            </div>
                            <div class="modal-body">
                                <pre class="bg-light p-3 rounded" style="max-height: 400px; overflow-y: auto;">${details}</pre>
                            </div>
                            <div class="modal-footer">
                                <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">关闭</button>
                                <button type="button" class="btn btn-primary" onclick="copyToClipboard('${details.replace(/'/g, "\\'")}')">复制</button>
                            </div>
                        </div>
                    </div>
                </div>
            `;
            
            document.body.insertAdjacentHTML('beforeend', modalHtml);
            const modalElement = document.body.lastElementChild;
            const modalInstance = new bootstrap.Modal(modalElement);
            modalInstance.show();
            
            // 模态框关闭后删除元素
            modalElement.addEventListener('hidden.bs.modal', function() {
                modalElement.remove();
            });
            
        } else {
            showNotification('获取固件详情失败: ' + result.error, 'error');
        }
        
    } catch (error) {
        console.error('获取固件详情失败:', error);
        showNotification('获取固件详情失败: ' + error.message, 'error');
    }
}

// 复制固件信息
async function copyFirmwareInfo(firmwareId) {
    try {
        const response = await fetch(`/api/v1/firmwares/${firmwareId}`);
        const result = await response.json();
        
        if (result.success) {
            const firmware = result.data;
            const info = `固件名称: ${firmware.original_filename}
版本号: ${firmware.version}
文件ID: ${firmware.id}
文件大小: ${formatFileSize(firmware.size)}
上传时间: ${new Date(firmware.upload_time).toLocaleString('zh-CN')}
目标设备: ${firmware.target_device}
加密状态: ${firmware.is_encrypted ? '已加密' : '未加密'}
校验码: ${firmware.checksum}`;
            
            await copyToClipboard(info);
            showNotification('固件信息已复制到剪贴板', 'success');
        } else {
            showNotification('获取固件信息失败: ' + result.error, 'error');
        }
        
    } catch (error) {
        console.error('复制固件信息失败:', error);
        showNotification('复制失败: ' + error.message, 'error');
    }
}

// 导出固件列表
async function exportFirmwareList() {
    try {
        const response = await fetch('/api/v1/firmwares');
        const result = await response.json();
        
        if (result.success) {
            const firmwares = result.data.firmwares;
            
            // 转换为CSV格式
            const headers = ['固件名称', '版本号', '文件大小', '上传时间', '目标设备', '加密状态', '是否最新', '校验码'];
            const csvContent = [
                headers.join(','),
                ...firmwares.map(fw => [
                    `"${fw.original_filename}"`,
                    `"${fw.version}"`,
                    fw.size,
                    `"${new Date(fw.upload_time).toISOString()}"`,
                    `"${fw.target_device}"`,
                    fw.is_encrypted ? '已加密' : '未加密',
                    fw.is_latest ? '是' : '否',
                    `"${fw.checksum}"`
                ].join(','))
            ].join('\n');
            
            // 创建下载链接
            const blob = new Blob(['\ufeff' + csvContent], { type: 'text/csv;charset=utf-8;' });
            const link = document.createElement('a');
            link.href = URL.createObjectURL(blob);
            link.download = `firmware_list_${new Date().toISOString().split('T')[0]}.csv`;
            link.click();
            
            showNotification('固件列表导出成功', 'success');
        } else {
            showNotification('导出失败: ' + result.error, 'error');
        }
        
    } catch (error) {
        console.error('导出固件列表失败:', error);
        showNotification('导出失败: ' + error.message, 'error');
    }
}

// 工具函数
function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

async function copyToClipboard(text) {
    try {
        await navigator.clipboard.writeText(text);
    } catch (err) {
        // 备用方法
        const textArea = document.createElement('textarea');
        textArea.value = text;
        document.body.appendChild(textArea);
        textArea.select();
        document.execCommand('copy');
        document.body.removeChild(textArea);
    }
}

function showNotification(message, type = 'info') {
    // 创建通知元素
    const notification = document.createElement('div');
    notification.className = `alert alert-${type === 'error' ? 'danger' : type} alert-dismissible fade show position-fixed`;
    notification.style.cssText = 'top: 20px; right: 20px; z-index: 9999; min-width: 300px;';
    notification.innerHTML = `
        ${message}
        <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
    `;
    
    document.body.appendChild(notification);
    
    // 3秒后自动删除
    setTimeout(() => {
        if (notification.parentNode) {
            notification.remove();
        }
    }, 3000);
}