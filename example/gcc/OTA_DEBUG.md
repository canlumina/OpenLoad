# OTA调试指南

## 问题现象
- WiFi连接成功，IP获取正常
- TCP连接超时，无法下载固件
- 服务器日志显示有请求到达

## 调试步骤

### 1. 使用调试命令（od）
```
> od
```
这个命令会：
- 测试TCP连接
- 发送HTTP请求
- 显示原始响应数据
- 解析+IPD格式

### 2. 检查ESP8266固件版本
```
> wi
```
查看ESP8266固件版本，确保支持HTTP功能。

### 3. 可能的问题原因

#### A. ESP8266固件问题
某些ESP8266固件版本在处理HTTP响应时有问题。如果调试显示没有收到+IPD数据，可能需要：
- 更新ESP8266固件
- 使用AT+CIUPDATE命令更新
- 或手动刷新固件

#### B. 接收缓冲区问题
如果收到部分数据但解析失败：
- 检查缓冲区大小
- 增加接收超时时间

#### C. 服务器端口问题
确保服务器端口3685未被防火墙阻挡。

## 修复方案

### 方案1：更新ESP8266固件
```
1. 下载最新ESP8266固件
2. 使用ESP8266刷写工具更新
3. 推荐版本：AT version 1.7.x或更高
```

### 方案2：使用透传模式
如果普通模式不工作，可以尝试透传模式：
```
AT+CIPMODE=1  // 设置透传模式
AT+CIPSEND    // 开始透传
// 发送HTTP请求
+++           // 退出透传
```

### 方案3：降级HTTP协议
修改HTTP请求为HTTP/1.0：
```
GET /path HTTP/1.0\r\n
Host: server\r\n
\r\n
```

## 测试服务器连接

### 使用curl测试（PC端）
```bash
curl http://115.190.137.231:3685/api/firmware/download/latest
```

### 使用telnet测试
```bash
telnet 115.190.137.231 3685
GET /api/firmware/download/latest HTTP/1.1
Host: 115.190.137.231
Connection: close

```

## 已知问题和解决方案

### 问题1：+IPD数据解析失败
**症状**：收到数据但无法解析
**解决**：已修复esp8266.c中的+IPD解析逻辑

### 问题2：HTTP响应超时
**症状**：连接成功但接收超时
**解决**：增加超时时间到10秒

### 问题3：TCP连接失败
**症状**：AT+CIPSTART返回ERROR
**可能原因**：
- WiFi未连接
- 服务器不可达
- ESP8266忙碌

## 建议的ESP8266配置
```
ATE0             // 关闭回显
AT+CWMODE=1      // Station模式
AT+CIPMUX=0      // 单连接
AT+CIPMODE=0     // 普通模式
AT+SLEEP=0       // 禁用休眠
```

## 调试输出示例
正常情况下应该看到：
```
TCP connected successfully!
Got +IPD response!
HTTP Status Code: 200
Content-Length: xxxxx bytes
```

如果看不到+IPD响应，说明ESP8266可能：
1. 固件版本过旧
2. 配置不正确
3. 硬件问题