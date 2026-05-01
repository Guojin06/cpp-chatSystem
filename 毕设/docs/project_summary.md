# DTU IoT 网关毕设项目 —— 开发文档

> 最后更新：2026-03-08

---

## 一、项目概述

**项目名称**：基于 DL/T 645 协议的 DTU 物联网网关

**核心目标**：实现一个轻量级的电表数据采集网关，从电表通过串口读取 DL/T 645 协议报文，解析后通过 HTTP API 提供给前端展示，同时支持本地缓存和云端上报。

**技术栈**：

| 层级 | 技术 | 路径 |
|------|------|------|
| C++ 核心层 | C++17，Linux termios，CMake | `dtu-core/` |
| Python API 层 | Python 3，http.server | `api-server/` |
| Vue 前端层 | Vue 3，Element Plus，Vite | `meter-monitor/` |

---

## 二、项目目录结构

```
毕设/
├── dtu-core/                    # C++ 核心程序
│   ├── CMakeLists.txt           # 构建配置
│   ├── config/
│   │   └── config.json          # 运行时配置（轮询间隔、串口路径等）
│   ├── include/
│   │   ├── dlt645_parser.h      # 645 协议解析器头文件
│   │   └── serial_reader.h      # 串口读取器头文件
│   ├── src/
│   │   ├── main.cpp             # 主程序入口
│   │   ├── dlt645_parser.cpp    # 645 协议解析器实现
│   │   └── serial_reader.cpp    # 串口读取器实现
│   ├── tools/
│   │   └── meter_simulator.py   # Python 电表模拟器
│   └── build/                   # 构建输出目录
│
├── api-server/                  # Python HTTP API
│   ├── app/
│   │   └── simple_server.py     # HTTP 服务器（读 JSON，透传给前端）
│   └── data/
│       └── latest.json          # C++ 写入的最新数据
│
├── meter-monitor/               # Vue 3 前端
│   ├── src/
│   │   └── App.vue              # 主页面组件
│   ├── vite.config.js           # Vite 配置（含开发代理）
│   └── package.json
│
└── docs/
    └── project_summary.md       # 本文档
```

---

## 三、数据流架构

```
                         ┌──────────────────────────────┐
                         │    Python 模拟器（开发阶段）     │
                         │  tools/meter_simulator.py     │
                         └────────────┬─────────────────┘
                                      │ 写入 645 报文
                                      ▼
                              /tmp/vserial0 (虚拟串口写端)
                                      │
                              ┌───────┴────────┐
                              │  socat 虚拟串口  │
                              └───────┬────────┘
                                      │
                              /tmp/vserial1 (虚拟串口读端)
                                      │
                                      ▼
┌────────────────────────────────────────────────────────────────────┐
│                        C++ 核心程序 (dtu_core)                       │
│                                                                    │
│   1. 读取 config.json（轮询间隔、串口路径）                            │
│   2. 打开串口 SerialReader::open()                                  │
│   3. 循环：                                                         │
│      a. SerialReader::readFrame() 从串口读一帧                       │
│      b. DLT645Parser::parseFrame() 解析报文                         │
│      c. writeLatestJson() 写入 latest.json                          │
│      d. sleep(pollInterval)                                        │
└────────────────────────┬───────────────────────────────────────────┘
                         │ 写入 JSON 文件
                         ▼
              api-server/data/latest.json
                         │
                         ▼
┌────────────────────────────────────────────────────────────────────┐
│                  Python API 服务器 (simple_server.py)                │
│                                                                    │
│   GET /api/latest  → 读取 latest.json 返回                          │
│   GET /api/health  → 返回 {"status": "ok"}                         │
│   GET /api/config  → 读取 config.json 返回                          │
│                                                                    │
│   监听端口：127.0.0.1:8000                                          │
│   CORS 头：Access-Control-Allow-Origin: *                           │
└────────────────────────┬───────────────────────────────────────────┘
                         │ HTTP JSON 响应
                         ▼
┌────────────────────────────────────────────────────────────────────┐
│             Vite 开发服务器 (localhost:5173)                          │
│                                                                    │
│   proxy: /api/* → http://127.0.0.1:8000                            │
│   （生产环境改用 Nginx 做反向代理）                                     │
└────────────────────────┬───────────────────────────────────────────┘
                         │
                         ▼
┌────────────────────────────────────────────────────────────────────┐
│                     Vue 前端 (App.vue)                               │
│                                                                    │
│   每 5 秒 fetch('/api/latest')                                      │
│   显示：电压、电流、功率因数、有功功率、电能数据                          │
│   UI 框架：Element Plus                                             │
└────────────────────────────────────────────────────────────────────┘
```

---

## 四、已完成模块详解

### Phase 1：配置文件读取

**改动文件**：`dtu-core/src/main.cpp`、`dtu-core/config/config.json`

**实现内容**：
- `readFile()` 函数：读取文件内容到 `std::string`
- `parsePollInterval()` 函数：从 JSON 字符串中用 `find/substr/stoi` 提取轮询间隔
- `main()` 中使用配置值替代硬编码的 sleep 时长

**关键知识点**：
- `std::ifstream` 文件读取
- `std::string::find()` 返回 `std::string::npos` 表示未找到
- `std::stoi()` 自动跳过空格，将字符串转为整数
- 不依赖第三方 JSON 库，用纯字符串操作解析

**config.json 格式**：
```json
{
  "meter_id": "000000000001",
  "poll_interval_sec": 10,
  "serial_port": "/tmp/vserial1"
}
```

---

### Phase 2：新增数据项全链路（功率因数）

**改动文件**：4 个文件，跨 3 层

| 文件 | 改动 |
|------|------|
| `dlt645_parser.h` | `MeterReading` 结构体增加 `double powerFactor` 字段 |
| `dlt645_parser.cpp` | `parseFrame()` 中赋值 `reading.powerFactor = 0.85`（临时 mock） |
| `main.cpp` | `writeLatestJson()` 输出 `power_factor` 字段；控制台打印 |
| `App.vue` | `normalizeLatestData()` 读取 `payload.data.power_factor` |

**关键知识点**：
- 新增一个数据项需要改动每一层（解析→输出→API→前端），理解全链路意识
- `el-statistic` 组件需要 `:precision` 属性才能显示小数

---

### Phase 3：虚拟串口 + 串口读取模块

**新增文件**：

| 文件 | 作用 |
|------|------|
| `include/serial_reader.h` | `SerialReader` 类声明 |
| `src/serial_reader.cpp` | 串口打开、配置、逐字节读取帧 |
| `tools/meter_simulator.py` | 模拟电表，每 5 秒发送一帧 645 报文 |

**改动文件**：
- `main.cpp`：新增 `parseSerialPort()` 读取串口配置；`main()` 中支持串口/mock 双模式
- `CMakeLists.txt`：新增 `serial_reader.cpp` 编译
- `config.json`：新增 `serial_port` 字段

**串口读取核心逻辑（`readFrame()`）**：
```
1. 逐字节从串口 read()
2. 等到 0x68（帧头）开始收集
3. 收到 0x16（帧尾）且长度 ≥ 12 且第 8 字节是 0x68 → 完整帧
4. 超时返回空
```

**关键知识点**：
- Linux 一切皆文件：串口设备 = 文件，用 `open/read/close` 操作
- `termios` API 配置串口参数：波特率、数据位(8)、校验位(N)、停止位(1) → "8N1"
- `socat` 创建虚拟串口对：`/tmp/vserial0` 和 `/tmp/vserial1` 互通
- 模式切换设计：config 配了串口就用串口，没配就用 mock，串口打开失败也回退 mock

---

## 五、Bug 修复记录

### Bug 1：JSON 写入错误路径

- **现象**：API 返回的数据缺少 `power_factor`
- **原因**：`writeLatestJson()` 中的相对路径 `../api-server/data/latest.json` 从 `build/` 目录执行时指向了 `dtu-core/api-server/`（错误），而非 `毕设/api-server/`（正确）
- **修复**：改为 `../../api-server/data/latest.json`（退两层）
- **教训**：**相对路径以程序运行时的工作目录（cwd）为基准，不是源码位置**

### Bug 2：前端无法请求到 API

- **现象**：前端数据全显示 0
- **原因**：`API_BASE = 'http://127.0.0.1:8000'`，浏览器中 `127.0.0.1` 指向用户本机，而非服务器
- **修复**：
  1. `vite.config.js` 添加 proxy：`/api` → `http://127.0.0.1:8000`
  2. `App.vue` 中 `API_BASE` 改为空字符串
- **教训**：**`127.0.0.1` 指向"代码执行所在的机器"，浏览器里执行 = 用户电脑**

### Bug 3：el-statistic 不显示小数

- **现象**：22.2V 显示为 22V，0.12A 显示为 0A，0.85 显示为 0
- **原因**：Element Plus 的 `el-statistic` 默认截断小数
- **修复**：加 `:precision` 属性（电压 1 位、电流 2 位、功率因数 2 位）
- **教训**：**UI 组件的默认行为要查文档**

---

## 六、核心 C++ 知识点汇总

| 知识点 | 出现位置 | 说明 |
|--------|----------|------|
| `std::optional` | `parseFrame()` 返回值 | 表示"可能有值，可能没有"，替代返回指针或错误码 |
| `std::vector` 范围构造 | 数据域切片 | `vector(begin+4, begin+6)` 提取子数组 |
| `std::string::find/substr/stoi` | 配置解析 | 不依赖第三方库的 JSON 简易解析 |
| `std::ifstream` + `std::getline` | 文件读取 | 逐行读文件到字符串 |
| `std::filesystem` | 路径操作、创建目录 | C++17 文件系统库 |
| `static_cast<uint8_t>` | 减 0x33 操作 | 显式类型转换，避免隐式转换警告 |
| 匿名命名空间 `namespace {}` | main.cpp 辅助函数 | 限制函数可见性为本文件，类似 C 的 `static` |
| `termios` API | serial_reader.cpp | Linux 串口配置（波特率、8N1、原始模式） |
| `open/read/close` 系统调用 | serial_reader.cpp | Linux 一切皆文件，操作设备 = 操作文件 |

---

## 七、编译与运行

### 编译 C++ 核心
```bash
cd dtu-core/build
cmake ..
make
```

### 启动虚拟串口（开发环境）
```bash
socat -d -d pty,raw,echo=0,link=/tmp/vserial0 pty,raw,echo=0,link=/tmp/vserial1
```

### 启动模拟器
```bash
python3 dtu-core/tools/meter_simulator.py /tmp/vserial0
```

### 启动 C++ 核心
```bash
cd dtu-core/build
./dtu_core
```

### 启动 Python API
```bash
cd api-server
python3 app/simple_server.py
```

### 启动 Vue 前端
```bash
cd meter-monitor
npm run dev
```

### 一键启动顺序
```
1. socat（虚拟串口）
2. meter_simulator.py（模拟器）
3. ./dtu_core（C++ 核心）
4. simple_server.py（API 服务器）
5. npm run dev（前端）
```

---

## 八、后续开发计划

| Phase | 内容 | 预计工时 | 状态 |
|-------|------|----------|------|
| ~~1~~ | ~~配置文件读取~~ | ~~1h~~ | ✅ 已完成 |
| ~~2~~ | ~~新增数据项全链路~~ | ~~2h~~ | ✅ 已完成 |
| ~~3~~ | ~~虚拟串口 + 串口读取~~ | ~~3h~~ | ✅ 已完成 |
| **4** | **SQLite 本地缓存** | 3h | 待做 |
| **5** | **MQTT 发布到云端** | 3h | 待做 |
| **6** | **日志系统（spdlog）** | 1.5h | 待做 |
| **7** | **整体测试 + 文档 + 答辩** | 4h | 待做 |

### Phase 4：SQLite 本地缓存（断网保护）

**目标**：每次采集的数据写入 SQLite 数据库，网络断开时数据不丢失，恢复后可补传。

**技术要点**：
- 使用 SQLite3 C API（`sqlite3_open`, `sqlite3_exec`）
- 表结构：`readings(id, meter_id, timestamp, voltage, current, active_energy, power_factor, uploaded)`
- `uploaded = 0` 表示未上传，网络恢复后批量上传并标记为 1
- 这是论文实验数据的基础（对比断网场景下的数据完整性）

### Phase 5：MQTT 发布到云端

**目标**：将采集数据通过 MQTT 协议发布到云端 IoT 平台（如 EMQX、阿里云 IoT）。

**技术要点**：
- 使用 `libmosquitto` 或 `paho-mqtt-c` 库
- 发布 topic：`dtu/meter/{meter_id}`
- JSON 格式与 `latest.json` 一致
- 结合 Phase 4 的 `uploaded` 标记实现断网重传

### Phase 6：日志系统

**目标**：替换 `std::cout/cerr` 为结构化日志，支持日志级别和文件输出。

**技术要点**：
- 使用 `spdlog` 库（C++ 最流行的日志库）
- 日志级别：`debug`, `info`, `warn`, `error`
- 同时输出到控制台和文件

### Phase 7：整体测试 + 答辩准备

**目标**：
- 端到端测试：模拟器 → 串口 → 解析 → SQLite → MQTT → 前端
- 异常测试：串口断开、网络中断、配置错误
- 撰写毕业论文 / 答辩 PPT
- 能口述完整数据链路和每个模块的设计决策

---

## 九、DL/T 645 协议报文格式参考

```
┌──────┬──────────────┬──────┬──────┬──────┬──────────────┬──────┬──────┐
│ 帧头  │   地址域      │ 帧头  │控制码│数据长度│    数据域      │校验码│ 帧尾  │
│ 0x68 │  6 字节 BCD   │ 0x68 │ 1B   │ 1B   │  N 字节       │ 1B  │ 0x16 │
└──────┴──────────────┴──────┴──────┴──────┴──────────────┴──────┴──────┘
```

- **地址域**：电表编号，BCD 编码，低字节在前
- **控制码**：0x91 = 从站正常应答读数据
- **数据域**：每字节在传输前 +0x33，接收后 -0x33 还原
- **校验码**：从帧头到数据域最后一字节的累加和，取低 8 位
- **数据域内容**：`[数据标识 4B] [电压 2B] [电流 2B] [有功电能 2B]`，均为 BCD 编码

---

## 十、面试高频知识点（本项目相关）

1. **Linux 一切皆文件**：串口、管道、socket 都是文件描述符，用统一的 `open/read/write/close` 操作
2. **前后端分离部署**：开发用 Vite proxy，生产用 Nginx 反向代理
3. **相对路径 vs 绝对路径**：相对路径以运行时 cwd 为基准
4. **127.0.0.1 的含义**：回环地址，指向"当前执行代码的机器"
5. **策略模式**：同一接口（获取帧数据）可有多种实现（mock / 串口），通过配置切换
6. **std::optional**：C++17 的"有值或无值"语义，替代裸指针和错误码
7. **BCD 编码**：用 4 位二进制表示 1 位十进制，常见于电力/金融协议
8. **串口通信 8N1**：8 数据位、无校验位、1 停止位，最常见配置
9. **CORS**：跨域资源共享，浏览器安全策略，后端需返回 `Access-Control-Allow-Origin` 头
10. **termios**：Linux 终端/串口配置 API，控制波特率、数据位、原始模式等
