# STM32 OTA功能使用说明

## 概述
本项目实现了基于ESP8266的STM32 OTA（Over-The-Air）固件更新功能，支持通过WiFi从远程服务器下载固件并更新。

## OTA服务器信息
- 服务器IP: 115.190.137.231
- 服务端口: 3685
- API基础URL: http://115.190.137.231:3685

## 功能特性
1. **WiFi连接管理** - 支持连接/断开WiFi
2. **HTTP下载** - 通过HTTP协议下载固件
3. **双目标支持** - 可下载到内部Flash或外部Flash
4. **进度显示** - 实时显示下载进度
5. **固件验证** - 下载完成后自动验证固件完整性
6. **断线重连** - 支持下载过程中断线自动重连

## 使用步骤

### 1. 进入Bootloader命令模式
上电后3秒内按任意键进入bootloader命令界面

### 2. 连接WiFi（命令：w）
```
> w
=== WiFi Management ===
1. Connect to WiFi
2. Disconnect WiFi  
3. Initialize ESP8266
Select option: 1

Enter WiFi SSID: your_wifi_name
Enter Password: ********
```

### 3. 执行OTA更新（命令：o）
```
> o
=== OTA Firmware Update ===
Update target:
1. Internal Flash  
2. External Flash
Select: 1

Enter firmware URL (or 'default' for test server): default
```

### 4. OTA高级功能（命令：o后选择高级选项）
- **检查最新固件** - 获取服务器上最新固件信息
- **列出所有版本** - 查看服务器上所有可用固件版本
- **下载指定版本** - 下载特定版本的固件

## 快捷方式

### 下载最新固件到内部Flash
```
输入: latest 或 default
```

### 下载指定版本
```
输入: v1.0.0 或直接输入版本号
```

### 完整URL
```
输入: http://115.190.137.231:3685/api/firmware/download/v1.0.0
```

## API接口说明

### 获取最新固件信息
```
GET /api/firmware/latest
```

### 获取固件列表
```
GET /api/firmware/list
```

### 下载固件
```
GET /api/firmware/download/{version}
GET /api/firmware/download/latest
```

## 注意事项

1. **WiFi连接** - 执行OTA前必须先连接WiFi
2. **电源稳定** - OTA过程中请保持电源稳定，避免断电
3. **固件备份** - 建议先备份当前固件到外部Flash
4. **验证失败** - 如果固件验证失败，系统将保持在bootloader中

## 故障排除

### WiFi连接失败
- 检查SSID和密码是否正确
- 确认ESP8266模块正常工作（使用wi命令查看状态）
- 尝试重新初始化ESP8266（w命令选项3）

### OTA下载失败
- 检查WiFi连接状态
- 确认服务器地址和端口正确
- 检查服务器是否在线
- 尝试使用较小的chunk_size

### 固件验证失败
- 检查下载的固件是否完整
- 确认固件格式正确（bin文件）
- 验证固件起始地址正确

## 开发者信息

### 编译项目
```bash
cmake --preset=Release
cmake --build build/Release
```

### 添加新固件到服务器
访问 http://115.190.137.231:3685 上传新固件

### 自定义OTA服务器
修改 `Core/Inc/ota_manager.h` 中的服务器配置：
```c
#define OTA_SERVER_HOST    "your_server_ip"
#define OTA_SERVER_PORT    your_port
#define OTA_SERVER_URL     "http://your_server_ip:port"
```

## 版本历史
- v1.0.0 - 初始版本，支持基本OTA功能
- v1.1.0 - 添加断线重连和进度显示
- v1.2.0 - 支持外部Flash和多版本管理