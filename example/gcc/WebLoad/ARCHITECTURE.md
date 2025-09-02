# WebLoad 系统架构设计

## 系统概述

WebLoad是一个三层架构的固件管理系统，支持STM32设备的远程固件更新和管理。

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Web客户端     │    │   Qt客户端      │    │   移动端(未来)   │
│   (轻量级)      │    │   (功能完整)     │    │                │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          └──────────┬───────────┴──────────────────────┘
                     │ REST API (HTTP/HTTPS)
          ┌──────────▼───────────┐
          │     服务器端         │
          │   (Flask + API)     │
          └──────────┬───────────┘
                     │
          ┌──────────▼───────────┐
          │     STM32设备        │
          │   (串口/OTA通信)     │
          └─────────────────────┘
```

## 核心组件

### 1. 服务器端 (Flask)

**核心职责**：
- 统一的REST API接口
- 固件存储和版本管理
- 加密/解密处理
- 设备通信管理(串口/OTA)
- 用户认证和权限控制

**主要模块**：
```
backend/
├── app.py              # 主应用入口
├── api/
│   ├── __init__.py
│   ├── auth.py         # 用户认证
│   ├── device.py       # 设备管理
│   ├── firmware.py     # 固件管理
│   └── system.py       # 系统管理
├── core/
│   ├── __init__.py
│   ├── serial_manager.py    # 串口管理器
│   ├── crypto.py           # 加密处理
│   ├── firmware_manager.py # 固件管理器
│   └── ota_manager.py      # OTA管理器
├── models/
│   ├── __init__.py
│   ├── device.py       # 设备模型
│   ├── firmware.py     # 固件模型
│   └── user.py         # 用户模型
└── utils/
    ├── __init__.py
    ├── xmodem.py       # XMODEM协议
    └── logger.py       # 日志管理
```

### 2. Web客户端

**核心职责**：
- 轻量级固件管理界面
- 固件上传和基本信息展示
- 加密参数配置
- 远程固件更新触发

**特性**：
- 无需本地串口权限
- 跨平台浏览器支持
- 响应式设计
- 实时状态监控

### 3. Qt客户端

**核心职责**：
- 完整功能的桌面应用
- 本地串口调试功能
- 固件开发调试工具
- 批量设备管理

**特性**：
- 原生串口通信
- 实时终端交互
- 文件拖放支持
- 系统集成

## API设计

### RESTful API 规范

```
基础路径: /api/v1

认证相关:
POST /auth/login          # 用户登录
POST /auth/logout         # 用户登出
GET  /auth/profile        # 获取用户信息

设备管理:
GET    /devices           # 获取设备列表
GET    /devices/{id}      # 获取设备详情
POST   /devices/{id}/connect    # 连接设备
DELETE /devices/{id}/disconnect # 断开设备
POST   /devices/{id}/command    # 发送命令

固件管理:
GET    /firmwares         # 获取固件列表
POST   /firmwares         # 上传固件
GET    /firmwares/{id}    # 获取固件详情
DELETE /firmwares/{id}    # 删除固件
POST   /firmwares/{id}/deploy   # 部署固件

加密管理:
GET    /crypto/algorithms # 获取支持的加密算法
POST   /crypto/encrypt    # 加密固件
POST   /crypto/decrypt    # 解密固件
```

### 数据模型

```json
// 设备模型
{
  "id": "device_001",
  "name": "STM32F103ZET6_001",
  "type": "STM32F103ZET6",
  "status": "connected|disconnected|updating",
  "connection": {
    "type": "serial|ota",
    "port": "COM3",
    "baudrate": 115200,
    "ip": "192.168.1.100"
  },
  "bootloader": {
    "version": "2.0",
    "features": ["xmodem", "ota", "aes"]
  },
  "firmware": {
    "version": "1.2.3",
    "size": 65536,
    "checksum": "abc123",
    "encrypted": true
  }
}

// 固件模型
{
  "id": "fw_001",
  "name": "app_v1.2.3.bin",
  "version": "1.2.3",
  "size": 65536,
  "checksum": "abc123",
  "upload_time": "2024-09-02T12:00:00Z",
  "encryption": {
    "algorithm": "aes-128-cbc",
    "key": "encrypted_key_here"
  },
  "metadata": {
    "target": "STM32F103ZET6",
    "build_time": "2024-09-02T10:00:00Z",
    "compiler": "arm-gcc-10.3.1"
  }
}
```

## 安全设计

### 1. 通信安全
- HTTPS/WSS加密传输
- API密钥认证
- JWT会话管理

### 2. 固件安全
- 支持多种加密算法 (AES-128-CBC, XOR)
- 密钥安全存储
- 固件完整性校验

### 3. 访问控制
- 用户角色权限管理
- 设备访问授权
- 操作审计日志

## 部署架构

### 1. 开发环境
```
localhost:5000  - Flask开发服务器
localhost:8080  - Qt客户端调试
```

### 2. 生产环境
```
nginx (反向代理) -> gunicorn (Flask应用) -> Redis (缓存) -> SQLite/PostgreSQL (数据库)
```

## 扩展性设计

### 1. 插件系统
- 支持自定义加密算法插件
- 支持自定义通信协议插件
- 支持自定义设备类型插件

### 2. 多设备支持
- 设备发现和管理
- 批量操作支持
- 设备分组管理

### 3. 云服务集成
- 固件云存储
- 远程设备管理
- 设备状态监控

## 技术栈

### 服务器端
- **后端框架**: Flask 2.3+
- **数据库**: SQLite (开发) / PostgreSQL (生产)
- **缓存**: Redis
- **任务队列**: Celery
- **认证**: Flask-JWT-Extended

### Web客户端
- **前端框架**: Bootstrap 5 + 原生JavaScript
- **构建工具**: 无需构建 (直接运行)
- **通信**: Fetch API + WebSocket

### Qt客户端
- **界面框架**: Qt6 + QML
- **网络**: Qt Network
- **串口**: Qt SerialPort
- **构建工具**: CMake + Qt Creator

## 开发计划

### Phase 1: 核心功能重构
1. 优化服务器端架构
2. 完善API设计
3. 改进固件管理

### Phase 2: 安全增强
1. 实现用户认证系统
2. 完善加密功能
3. 添加操作审计

### Phase 3: 客户端开发
1. 优化Web客户端
2. 开发Qt客户端
3. 移动端支持 (未来)

### Phase 4: 企业级特性
1. 多设备管理
2. 云服务集成
3. 监控和告警