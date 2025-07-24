# OpenLoad

## 概述
这是一个高度可移植的通用单片机Bootloader框架，支持多种MCU架构（包括ARM Cortex-M, RISC-V, AVR, PIC等）。设计核心是**硬件抽象层(HAL)** 架构，使得同一套代码可以轻松适配不同单片机平台，无需重写核心逻辑。

## 设计特点
- **跨平台支持**：通过HAL接口实现硬件无关性
- **模块化架构**：各功能组件解耦，便于定制
- **安全机制**：支持CRC校验、AES加密和数字签名
- **多通信接口**：UART/USB/CAN/Ethernet/无线兼容
- **轻量高效**：核心代码<8KB，RAM占用<1KB
- **可靠更新**：双区备份和断电保护机制

## 系统架构
```mermaid
graph TD
    A[Bootloader核心] --> B[硬件抽象层]
    A --> C[通信协议层]
    A --> D[安全模块]
    B --> E[MCU平台1]
    B --> F[MCU平台2]
    B --> G[MCU平台N]
    C --> H[UART]
    C --> I[USB]
    C --> J[CAN]
    D --> K[CRC校验]
    D --> L[AES加密]
    D --> M[数字签名]
```



##  快速开始

### 1. 配置硬件抽象层

在`hal`目录中实现目标平台的HAL接口：

```c
// hal_template.c
void hal_init(void) {
    // 初始化时钟、内存、中断
}

int hal_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    // 实现Flash写入
}

int hal_comm_receive(uint8_t *buf, uint32_t timeout) {
    // 实现通信接口接收
}
```

### 2. 配置内存布局

修改`boot_config.h`:

```c
// STM32F4示例配置
#define APP_START_ADDR     0x08008000
#define FLASH_PAGE_SIZE    0x2000       // 16KB页
#define RAM_SIZE           0x20000      // 128KB RAM
#define BOOTLOADER_SIZE    0x8000       // 32KB Bootloader
```

### 3. 选择通信协议

在`protocols`目录中选择或实现通信协议：

```c
// protocols/xmodem.c
const Protocol xmodem_protocol = {
    .init = xmodem_init,
    .receive_packet = xmodem_receive,
    .send_ack = xmodem_ack
};
```

### 4. 编译烧录





## 工作流程

```mermaid
flowchart TD
    %% 纵向主干流程
    A[上电复位] --> B[初始化基础硬件]
    B --> C{应用程序有效?}
    C -->|有效| D{更新触发条件满足?}
    D -->|是| E[进入Boot模式]
    D -->|否| F[跳转到应用程序]
    C -->|无效| E
    E --> G[初始化通信接口]
    G --> H[发送就绪信号]
    H --> I{等待固件数据}
    I -->|收到数据包| J[解析协议头]
    J --> K{数据包校验通过?}
    K -->|是| L[写入存储器]
    K -->|否| M[请求重传]
    M --> I
    L --> N{最后数据包?}
    N -->|否| I
    N -->|是| O[验证固件完整性]
    O -->|验证通过| P[更新引导标志]
    O -->|验证失败| Q[发送错误报告]
    Q --> R[等待清除指令]
    R --> I
    P --> F
    
    %% 横向功能分组
    subgraph 硬件抽象层
        direction TB
        B1[时钟初始化] --> B2[内存初始化] --> B3[中断配置]
        G1[UART/USB] --> G2[CAN/Ethernet] --> G3[无线模块]
        L1[Flash驱动] --> L2[EEPROM驱动] --> L3[外部存储]
    end
    
    subgraph 安全模块
        direction LR
        O1[CRC校验] --> O2[AES解密] --> O3[数字签名验证]
    end
    
    subgraph 应用跳转流程
        direction TB
        F1[关闭外设] --> F2[禁用中断] --> F3[设置应用栈指针] --> F4[加载应用复位向量] --> F5[跳转到应用入口]
    end
    
    %% 连接关系
    B --> B1
    G --> G1
    L --> L1
    O --> O1
    F --> F1
    
    %% 样式定义
    style 硬件抽象层 fill:#e6f7ff,stroke:#0066cc,stroke-dasharray: 5 5
    style 安全模块 fill:#fff2e6,stroke:#ff9900,stroke-dasharray: 5 5
    style 应用跳转流程 fill:#e6ffe6,stroke:#00cc66,stroke-dasharray: 5 5
    
    style C fill:#ffcc99,stroke:#ff6600
    style D fill:#ffcc99,stroke:#ff6600
    style K fill:#ffcc99,stroke:#ff6600
    style N fill:#ffcc99,stroke:#ff6600
    style O fill:#ffcc99,stroke:#ff6600
    
    classDef main fill:#f0f8ff,stroke:#1e90ff;
    class A,B,E,G,H,I,J,L,P,F main;
```





## 安全特性

| 安全机制    | 功能描述             | 配置文件选项         |
| :---------- | :------------------- | :------------------- |
| CRC32校验   | 固件完整性验证       | `ENABLE_CRC_CHECK=1` |
| AES-256加密 | 固件传输加密         | `ENABLE_AES=1`       |
| ECDSA签名   | 固件来源认证         | `ENABLE_ECDSA=1`     |
| 防回滚保护  | 防止旧版本固件被安装 | `ENABLE_VERSION=1`   |
| 双区备份    | 安全更新和回退机制   | `ENABLE_DUAL_BANK=1` |



## 移植指南

1. **实现HAL接口**：
   - `hal_init()` - 硬件初始化
   - `hal_flash_*()` - Flash操作
   - `hal_comm_*()` - 通信接口
2. **配置内存布局**：
   - 在`boot_config.h`中定义应用起始地址
   - 设置Flash页大小和总容量
3. **选择通信协议**：
   - 内置支持XMODEM/YMODEM
   - 可扩展自定义协议
4. **定制安全选项**：
   - 根据需求启用安全特性
   - 配置加密密钥和证书



## 工具链支持

- **固件打包工具**：生成加密签名固件
- **Bootloader CLI**：命令行更新工具
- **QBoot助手**：图形化更新工具
- **日志分析器**：Bootloader调试工具



## 贡献指南

欢迎通过Issue和Pull Request贡献：

1. 新增MCU平台支持
2. 扩展通信协议
3. 增强安全功能
4. 优化文档和示例



## 许可证
Apache License 2.0 - 允许商业使用和修改

本项目中使用了以下第三方组件：
RT-Thread fal库
项目名称: fal
功能: 用于存储管理和分区管理
来源: 基于RT-Thread官方fal组件开发
LICENSE: RT-ThreadfalLICENSE
版权声明
本项目基于RT-Thread fal组件进行开发和优化，版权归RT-Thread所有。如需使用本项目中的fal相关功能，请遵守RT-Thread的LICENSE协议。