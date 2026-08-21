# MySQL RPC 微服务 — C++ 连接池版

> 基于 MySQL C API + Boost.Asio TCP + Protobuf 的独立 RPC 微服务，提供高性能的数据库连接池和任务分发机制。

![C++](https://img.shields.io/badge/C++-17-%2300599C?style=flat-square&logo=c%2B%2B)
![MySQL](https://img.shields.io/badge/MySQL-8-%234479A1?style=flat-square&logo=mysql)
![Boost](https://img.shields.io/badge/Boost-Asio-%23F6822B?style=flat-square&logo=boost)
![Protobuf](https://img.shields.io/badge/Protobuf-3-%234285F4?style=flat-square&logo=google)

---

## 📖 概述

MySQL RPC 微服务是 WebServer 架构中独立的**数据库接入服务**。它通过 **Boost.Asio TCP** 长连接接收其他微服务的请求，使用 **Protobuf** 序列化协议通信，将 MySQL 数据库操作封装为可编排的任务，并通过**连接池 + 线程池**实现高并发处理。

> **定位**：为其他微服务提供统一的数据库访问能力，隔离底层数据库实现细节，支持高并发访问场景。

---

## 🏗️ 架构设计

```
┌─────────────┐     TCP + Protobuf      ┌──────────────────────────┐
│  其他微服务  │ ──────────────────────► │     MySQL 微服务          │
└─────────────┘                          │  ┌────────────────────┐  │
                                         │  │  Boost.Asio TCP    │  │
                                         │  │  服务器（消息循环） │  │
                                         │  └────────┬───────────┘  │
                                         │           ▼              │
                                         │  ┌────────────────────┐  │
                                         │  │ 任务构建 BuildTask  │  │
                                         │  └────────┬───────────┘  │
                                         │           ▼              │
                                         │  ┌────────────────────┐  │
                                         │  │ 线程池 + 连接池     │  │
                                         │  │（限流/等待队列）    │  │
                                         │  └────────┬───────────┘  │
                                         │           ▼              │
                                         │  ┌────────────────────┐  │
                                         │  │  MySQL C API 执行  │  │
                                         │  └────────────────────┘  │
                                         └──────────────────────────┘
```

---

## ✨ 功能特性

- 🗄️ **连接池管理** — 支持初始连接数、最大连接数、连接超时、空闲回收、健康检查（Ping）
- ⏳ **等待队列限流** — 连接耗尽时请求排队等待，超过 `Wait_Queue_Max` 上限即拒绝（限流保护）
- 🧵 **线程池并发执行** — 可配置线程数，业务任务异步并发执行，避免阻塞主循环
- 🔐 **配置中心接入** — 由配置中心（ServiceID=4）下发 MySQL 连接参数，支持热更新配置
- 🛡️ **SQL 注入防护** — 使用 `mysql_real_escape_string` 对用户输入进行转义
- 🔄 **自动重连** — 监控线程定期巡检，断开的连接自动重建并回池
- 📊 **池状态监控** — 监控线程定期回收空闲的临时连接，维持合理连接水位
- 🔌 **TCP 长连接** — 基于 Boost.Asio 的独立 TCP RPC 服务，Protobuf 消息协议

---

## 📁 目录结构

```
MySQL/
├── source/
│   ├── main.cpp                    # 服务入口（默认端口 60908）
│   ├── CMakeLists.txt             # 构建配置（自动查找 vcpkg/MySQL）
│   ├── include/
│   │   ├── SQLWork.h              # 全局配置、服务器/任务声明
│   │   ├── MySQL_Connection.h     # 单条 MySQL 连接封装
│   │   └── MySQL_Pool.h           # 连接池、等待队列、监控线程
│   └── body/
│       ├── SQLWork.cpp            # 服务器主循环、任务构建、限流检查
│       ├── MySQL_Pool.cpp         # 连接池实现（预创建/回收/巡检）
│       └── MySQL_Connection.cpp   # MySQL C API 封装（CRUD/转义）
└── README.md
```

---

## 🧩 消息协议

基于 Protobuf 的 `sql_node` 消息，通过 TCP 传输：

| type | 含义             | 处理方式                              |
| ---- | ---------------- | ------------------------------------- |
| 0    | 握手信息         | 回复确认                              |
| 1    | 配置中心下发配置 | 校验后应用连接参数并初始化连接池      |
| >1   | 业务请求         | 构建任务 → 线程池 + 连接池执行 → 回复 |

### 配置消息（type=1）

由配置中心（`ConfigServiceID=4`）下发 `sql_init`：

```protobuf
message sql_init {
    string ip = 1;
    uint32 port = 2;
    string user = 3;
    string password = 4;
    string db = 5;
    uint32 initsize = 6;      // 初始连接数
    uint32 maxsize = 7;       // 最大连接数
    uint32 timemax = 8;       // 空闲回收时间
    uint32 timeout = 9;       // 连接超时
    uint32 threadpoolsize = 10; // 线程池线程数
}
```

---

## 🚀 快速开始

### 依赖

- C++17 编译器
- MySQL 8.0（含 `include/mysql.h` 与 `lib/libmysql.lib`）
- Boost.Asio
- Protobuf 3.x

### 构建

```bash
cd MySQL/source
cmake -B build -DMYSQL_ROOT="C:/Program Files/MySQL/MySQL Server 8.0"
cmake --build build --config Release
```

### 运行

```bash
./build/SQL.exe
# 服务启动，监听端口 60908，等待配置中心下发配置...
```

---

## 🔑 关键实现

### 连接池核心流程

```
获取连接 GetConnection()
  ├─ 队列有空闲？→ 直接取出
  ├─ 队列空且未达上限？→ 临时创建新连接
  └─ 队列空且已达上限？→ 返回 nullptr → 进入等待队列

归还连接 ReConnection()
  ├─ Ping 失败？→ 删除重建
  ├─ 有等待者？→ 连接所有权转移给等待回调
  └─ 无等待者？→ 入池并更新最后使用时间

监控线程 MonitorLoop()
  └─ 定时巡检（5~60s）→ 回收空闲超时的临时连接
```

### 限流保护

当连接池耗尽且等待队列超过 `Wait_Queue_Max`（默认 10）时，新的请求会立即收到 `"服务器繁忙，请稍后重试"` 的回复，保障服务稳定性。

---

## 📋 TODO

- [ ] 更多业务类型支持（注册、登录、设备管理等）
- [ ] 日志归档（`InsertLog`）
- [ ] 配置中心 Token 校验（当前仅校验 ServiceID）
- [ ] 用户密码加密存储
- [ ] 慢查询日志与统计
