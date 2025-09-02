# OpenLoad Web管理器

一个基于Flask的Web界面管理工具，用于STM32 OpenLoad bootloader的固件管理和设备控制。

## 🚀 主要功能

### 核心特性
- ✅ **串口连接管理** - 自动检测并连接STM32设备
- ✅ **固件上传管理** - 支持.bin固件文件上传和管理  
- ✅ **在线固件升级** - 支持XMODEM和OTA两种升级方式
- ✅ **加密支持** - 无加密/XOR加密/AES-128-CBC加密
- ✅ **实时串口终端** - 双向串口通信监控
- ✅ **设备信息查看** - 实时获取bootloader和设备状态
- ✅ **命令执行** - 图形化执行bootloader命令

### 技术特性
- **前端**: Bootstrap 5 + 原生JavaScript  
- **后端**: Python Flask + pySerial
- **实时通信**: 轮询机制实现准实时数据更新
- **跨平台**: 支持Windows/Linux/macOS

## 📁 项目结构

```
WebLoad/
├── backend/
│   └── app.py              # Flask后端API服务
├── frontend/               # (预留扩展目录)
├── static/
│   ├── css/
│   │   └── style.css       # 样式文件
│   └── js/
│       └── app.js          # 前端JavaScript逻辑
├── templates/
│   └── index.html          # 主界面模板
├── uploads/                # 固件上传目录(自动创建)
├── config.py               # 应用配置
├── requirements.txt        # Python依赖
├── run.py                  # 应用启动脚本
├── start.bat               # Windows启动脚本
├── start.sh                # Linux/macOS启动脚本
└── README.md               # 本文件
```

## 🔧 安装和使用

### 方式一：使用启动脚本 (推荐)

**Windows:**
```cmd
cd WebLoad
start.bat
```

**Linux/macOS:**
```bash
cd WebLoad
chmod +x start.sh
./start.sh
```

### 方式二：手动安装

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

# 4. 启动应用
python run.py
```

### 访问界面

打开浏览器访问: `http://localhost:5000`

## 📋 使用说明

### 1. 串口连接
1. 点击"串口连接"面板的下拉菜单选择STM32设备串口
2. 选择波特率(默认115200)
3. 点击"连接"按钮建立连接
4. 连接成功后状态栏显示"已连接"

### 2. 固件管理
1. **上传固件**: 在"固件上传"标签页选择.bin文件并上传
2. **文件管理**: 在"文件管理"标签页查看、下载或删除已上传的固件
3. **固件升级**: 在"固件升级"标签页配置升级参数并执行

### 3. 设备控制
- **设备信息**: 点击"刷新"获取最新的设备和bootloader信息
- **Bootloader命令**: 使用左侧命令按钮执行各种bootloader操作
- **串口终端**: 查看实时串口通信数据，手动发送命令

### 4. 固件升级流程
1. 确保设备已连接
2. 上传要升级的固件文件  
3. 在"固件升级"标签页选择:
   - 固件文件
   - 升级方式 (XMODEM/OTA)
   - 目标位置 (内部Flash/外部Flash)
   - 加密方式 (无加密/XOR/AES-128-CBC)
4. 点击"开始升级"并等待完成

## 🔧 配置说明

### 环境变量
```bash
# 服务器配置
FLASK_HOST=0.0.0.0          # 服务器地址
FLASK_PORT=5000             # 服务器端口  
FLASK_DEBUG=True            # 调试模式

# 安全配置  
SECRET_KEY=your_secret_key  # Flask密钥
```

### 命令行参数
```bash
python run.py --help           # 查看帮助
python run.py --production     # 生产模式
python run.py --host 0.0.0.0 --port 8080  # 自定义地址端口
```

## 🔌 API接口

### 串口管理
- `GET /api/serial/ports` - 获取可用串口列表
- `POST /api/serial/connect` - 连接串口
- `POST /api/serial/disconnect` - 断开串口  
- `GET /api/serial/status` - 获取连接状态
- `POST /api/serial/send` - 发送串口命令
- `GET /api/serial/messages` - 获取串口消息

### 设备管理
- `GET /api/device/info` - 获取设备信息
- `POST /api/bootloader/command/<command>` - 执行bootloader命令

### 固件管理
- `POST /api/firmware/upload` - 上传固件
- `GET /api/firmware/list` - 获取固件列表
- `GET /api/firmware/download/<filename>` - 下载固件
- `DELETE /api/firmware/delete/<filename>` - 删除固件
- `POST /api/firmware/update` - 开始固件升级
- `GET /api/firmware/progress` - 获取升级进度

## 🛠️ 开发说明

### 后端扩展
- 串口管理类: `SerialManager` (backend/app.py)
- 添加新的API端点遵循RESTful设计
- 使用Flask Blueprint组织大型项目

### 前端扩展  
- UI组件基于Bootstrap 5
- JavaScript使用ES6+语法
- AJAX请求使用原生fetch API

### 数据流
```
浏览器 <-> Flask API <-> pySerial <-> STM32设备
```

## 🔍 故障排除

### 常见问题

**1. 串口连接失败**
- 检查设备是否正确连接
- 确认串口未被其他程序占用
- 检查波特率设置是否正确

**2. 固件上传失败**  
- 确认文件格式为.bin
- 检查文件大小不超过16MB
- 查看uploads目录权限

**3. 升级失败**
- 确保设备处于bootloader模式
- 检查加密设置与固件匹配
- 查看串口终端的错误信息

**4. 网页无法访问**
- 检查防火墙设置
- 确认端口未被占用
- 查看控制台错误信息

### 调试模式
```bash
python run.py --debug
```

启用调试模式后可以:
- 查看详细错误信息
- 自动重载代码变更
- 使用浏览器开发工具调试

## 📚 相关文档

- [STM32 OpenLoad项目](../) - 主项目文档
- [AES固件加密指南](../AES_FIRMWARE_GUIDE.md) - 加密固件使用说明
- [Bootloader命令手册](../BOOTLOADER_README.md) - 命令详细说明

## 🤝 贡献指南

1. Fork本项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开Pull Request

## 📄 许可证

本项目基于MIT许可证开源 - 查看 [LICENSE](../LICENSE) 文件了解详情。

## 📞 支持

如有问题或建议，请:
- 提交Issue到项目仓库
- 发送邮件到项目维护者
- 查看项目Wiki获取更多信息

---

**版本**: v1.0.0  
**最后更新**: 2024年9月2日  
**作者**: yangcan