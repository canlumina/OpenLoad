# WebLoad 系统部署和使用指南

## 系统概述

WebLoad v2.0 采用全新的三层架构设计，提供了更强大和灵活的固件管理能力：

- **服务器端**: 基于Flask的RESTful API服务
- **Web客户端**: 轻量级浏览器界面
- **Qt客户端**: 功能完整的桌面应用 (计划开发)

## 🚀 快速开始

### 1. 环境要求

**Python环境**:
- Python 3.8+ 
- pip包管理器

**系统要求**:
- Windows/Linux/macOS
- 至少50MB存储空间
- 网络端口5000可用

### 2. 安装部署

#### 方法一: 自动安装脚本

**Windows:**
```batch
cd WebLoad
start.bat
```

**Linux/macOS:**
```bash
cd WebLoad
chmod +x start.sh
./start.sh
```

#### 方法二: 手动安装

```bash
# 1. 创建虚拟环境
python -m venv venv

# 2. 激活虚拟环境
# Windows:
venv\Scripts\activate
# Linux/macOS:
source venv/bin/activate

# 3. 安装依赖
pip install -r requirements.txt

# 4. 启动服务器
python run_new.py
```

### 3. 启动服务

```bash
# 开发模式 (推荐)
python run_new.py --debug

# 生产模式
python run_new.py --production

# 自定义地址端口
python run_new.py --host 0.0.0.0 --port 8080
```

### 4. 访问界面

打开浏览器访问: `http://localhost:5000`

## 📡 API 接口文档

### 基础信息

- **API版本**: v1
- **基础路径**: `/api/v1`
- **数据格式**: JSON
- **字符编码**: UTF-8

### 设备管理

```http
# 获取可用串口
GET /api/v1/devices/ports

# 获取设备列表
GET /api/v1/devices

# 连接设备
POST /api/v1/devices/connect
{
  "port": "COM3",
  "baudrate": 115200
}

# 断开设备
POST /api/v1/devices/disconnect
{
  "port": "COM3"
}

# 发送命令
POST /api/v1/devices/{device_id}/command
{
  "command": "i"
}

# Bootloader命令
POST /api/v1/devices/{device_id}/bootloader/{command}
```

### 固件管理

```http
# 获取固件列表
GET /api/v1/firmwares

# 上传固件
POST /api/v1/firmwares
Content-Type: multipart/form-data
firmware: [binary file]
version: "1.2.3"
target_device: "STM32F103ZET6"

# 获取固件详情
GET /api/v1/firmwares/{firmware_id}

# 下载固件
GET /api/v1/firmwares/{firmware_id}/download

# 删除固件
DELETE /api/v1/firmwares/{firmware_id}

# 加密固件
POST /api/v1/firmwares/{firmware_id}/encrypt
{
  "algorithm": "aes-128-cbc",
  "password": "mypassword"
}

# 部署固件
POST /api/v1/firmwares/{firmware_id}/deploy
{
  "device_id": "device_COM3",
  "method": "xmodem",
  "target": "internal"
}
```

### 加密管理

```http
# 获取支持的算法
GET /api/v1/crypto/algorithms

# 生成密钥
POST /api/v1/crypto/key/generate
{
  "algorithm": "aes-128-cbc",
  "password": "mypassword"
}

# 加密数据
POST /api/v1/crypto/encrypt
{
  "data": "48656c6c6f",
  "algorithm": "aes-128-cbc",
  "password": "mypassword"
}

# 解密数据
POST /api/v1/crypto/decrypt
{
  "encrypted_data": "...",
  "algorithm": "aes-128-cbc",
  "key": "...",
  "metadata": {...}
}
```

### 系统管理

```http
# 系统信息
GET /api/v1/system/info

# 健康状态
GET /api/v1/system/health

# 系统配置
GET /api/v1/system/config

# 系统统计
GET /api/v1/system/stats
```

## 🔧 使用指南

### Web端操作流程

#### 1. 设备连接
1. 选择串口端口
2. 设置波特率 (默认115200)
3. 点击"连接"建立通信
4. 状态栏显示连接状态

#### 2. 固件管理
1. **上传固件**:
   - 选择.bin文件
   - 填写版本号 (可选)
   - 点击上传

2. **加密固件**:
   - 选择加密算法 (AES-128-CBC推荐)
   - 输入密码或密钥
   - 执行加密

3. **部署固件**:
   - 选择目标固件
   - 选择部署方式 (XMODEM/OTA)
   - 选择存储位置 (内部/外部Flash)
   - 开始部署

#### 3. 设备控制
- **设备信息**: 查看MCU型号、Bootloader版本等
- **Bootloader命令**: 执行帮助、擦除、重启等操作
- **串口终端**: 实时查看通信数据

### 高级功能

#### 1. 批量操作
```javascript
// 批量删除固件
fetch('/api/v1/firmwares/batch/delete', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({
    firmware_ids: ['fw_001', 'fw_002', 'fw_003']
  })
})
```

#### 2. 固件版本管理
- 自动版本检测
- 版本比较和升级提醒
- 固件历史记录

#### 3. 安全特性
- AES-128/256-CBC加密
- 固件完整性校验
- 密钥安全存储

## 🔍 故障排除

### 常见问题

**1. 服务器启动失败**
```bash
# 检查Python版本
python --version  # 需要3.8+

# 检查端口占用
netstat -an | grep 5000

# 查看详细错误信息
python run_new.py --debug
```

**2. 串口连接失败**
- 确认设备已连接且驱动已安装
- 检查串口未被其他程序占用
- 尝试不同的波特率设置

**3. 固件上传失败**
- 检查文件格式是否为.bin
- 确认文件大小不超过16MB
- 查看上传目录权限

**4. 加密功能异常**
- 确认cryptography库已正确安装
- 检查密钥格式 (十六进制)
- 验证算法参数

**5. API调用错误**
```bash
# 测试API连接
curl -X GET http://localhost:5000/api/v1/system/health

# 查看服务器日志
python run_new.py --debug
```

### 调试模式

启用调试模式可获得更详细的错误信息：
```bash
python run_new.py --debug
```

调试模式特性：
- 详细的错误堆栈
- API请求/响应日志
- 自动重载代码变更
- 开发工具支持

## 📊 性能优化

### 1. 生产环境部署

```bash
# 使用gunicorn (推荐)
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 backend.app:app

# 使用nginx反向代理
upstream webload {
    server 127.0.0.1:5000;
}

server {
    listen 80;
    location / {
        proxy_pass http://webload;
    }
}
```

### 2. 数据库优化 (未来)

- 使用PostgreSQL替代SQLite
- 实现连接池管理
- 添加Redis缓存层

### 3. 并发优化

- 异步任务队列 (Celery)
- WebSocket实时通信
- 负载均衡支持

## 🔒 安全建议

### 1. 网络安全
- 使用HTTPS加密传输
- 配置防火墙规则
- 限制访问IP白名单

### 2. 认证授权
- 实现用户登录系统
- JWT令牌管理
- 角色权限控制

### 3. 数据安全
- 固件加密存储
- 密钥安全管理
- 操作审计日志

## 🧪 测试

### API测试
```bash
# 运行测试脚本
python test_api.py

# 手动测试单个接口
curl -X GET http://localhost:5000/api/v1/system/info
```

### 功能测试
1. 串口连接测试
2. 固件上传下载测试
3. 加密解密测试
4. 设备命令测试

## 📚 开发扩展

### 添加新的API端点
1. 在`backend/api/`目录创建新模块
2. 定义路由和处理函数
3. 在`__init__.py`中注册蓝图

### 自定义加密算法
1. 继承`CryptoManager`类
2. 实现加密/解密方法
3. 在算法列表中注册

### 数据库集成
1. 安装SQLAlchemy
2. 定义数据模型
3. 实现CRUD操作

## 📞 技术支持

- **文档**: 查看项目README和代码注释
- **问题报告**: 提交GitHub Issues
- **开发社区**: 参与项目讨论

---

**版本**: WebLoad v2.0.0  
**最后更新**: 2024年9月2日  
**作者**: yangcan