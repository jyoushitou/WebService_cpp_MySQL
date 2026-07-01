# MySQL RPC 微服务 — C++ 连接池版

> 基于 MySQL C API + Boost.Beast WebSocket 的独立 RPC 微服务，提供高性能的数据库连接池和 JSON-RPC 接口。

![C++](https://img.shields.io/badge/C++-17-%2300599C?style=flat-square&logo=c%2B%2B)
![MySQL](https://img.shields.io/badge/MySQL-8-%234479A1?style=flat-square&logo=mysql)
![Boost](https://img.shields.io/badge/Boost-Beast/Asio-%23F6822B?style=flat-square&logo=boost)
![WebSocket](https://img.shields.io/badge/WebSocket-JSON--RPC-%234285F4?style=flat-square)

---

## 📖 概述

MySQL RPC 微服务是 WebServer 架构中独立的**数据库接入服务**。它将 MySQL 数据库操作封装为 JSON-RPC 接口，通过 WebSocket 对外提供服务，并内置高性能**连接池**管理机制。

> **定位**：为其他微服务提供统一的数据库访问能力，隔离底层数据库实现细节，支持高并发访问场景。

---

## ✨ 功能特性

- 🗄️ **连接池管理** — 支持最小/最大连接数、超时控制、空闲回收、健康检查
- 🔌 **WebSocket JSON-RPC** — 基于 Boost.Beast 的独立 RPC 服务
- 📦 **用户管理** — 用户注册、登录鉴权、信息查询、设备管理
- 🧵 **多线程 I/O** — 可配置线程数，支持并发请求
- 🔄 **自动重连** — 连接池提供健康检查与自动恢复
- 📊 **池状态监控** — 活跃/空闲/总连接数实时查询

---

## 🏗️ 架构设计

```
┌────────────────────────────────────────────┐
│           MySQL RPC 微服务                  │
│                                            │
│  ┌──────────┐   ┌─────────────────────┐    │
│  │ WebSocket │   │   RPC 方法路由器     │    │
│  │  服务端   │──▶│  (RPCRouter)        │    │
│  │(Boost.    │   │  method → handler   │    │
│  │ Beast)    │   └──────────┬──────────┘    │
│  └──────────┘              │               │
│                            ▼               │
│  ┌─────────────────────────────────────┐   │
│  │         MySQL 连接池                │   │
│  │  ┌──────┐ ┌──────┐ ┌──────┐       │   │
│  │  │Conn 1│ │Conn 2│ │Conn 3│  ...  │   │
│  │  └──────┘ └──────┘ └──────┘       │   │
│  │  minSize=2  maxSize=10            │   │
│  └─────────────────────────────────────┘   │
└────────────────────────────────────────────┘
                     │
                     ▼
              ┌──────────────┐
              │    MySQL 8    │
              │  web_server   │
              └──────────────┘
```

---

## 📂 项目结构

```
MySQL/
├── source/
│   ├── include/                       # 头文件
│   │   ├── MyMySQL.h                  # MySQL 操作封装 + 连接池管理器
│   │   ├── ConnectionPool.h           # 连接池实现
│   │   ├── RPCConfig.h                # RPC 服务配置
│   │   ├── RPCRouter.h                # RPC 路由分发
│   │   ├── RPCBusiness.h              # 业务方法声明
│   │   ├── RPCProtocol.h              # RPC 协议定义
│   │   ├── WebSocketSession.h         # WebSocket 会话处理
│   │   └── Utils.h                    # 通用工具
│   └── body/                          # 源文件
│       ├── main.cpp                   # 程序入口
│       ├── mysql.cpp                  # MySQL 操作实现
│       ├── ConnectionPool.cpp         # 连接池实现
│       ├── RPCBusiness.cpp            # 业务方法实现
│       ├── Utils.cpp                  # 工具实现
│       └── WebSocketSession.cpp       # 会话处理实现
├── build/                             # 构建输出
├── CMakeLists.txt                     # CMake 构建配置
├── LICENSE                            # MIT 许可证
└── README.md                          # 本文件
```

---

## 🚀 快速开始

### 前置依赖

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| C++ 编译器 | C++17 (GCC 8+ / MSVC 2019+) | |
| CMake | 3.16+ | 构建系统 |
| Boost | 1.70+ | 需要 Beast、JSON |
| MySQL | 8.0+ | 需要 C API 开发库（libmysql） |

### MySQL 环境准备

```sql
-- 创建数据库和用户（需 MySQL root 权限）
CREATE DATABASE IF NOT EXISTS web_server DEFAULT CHARSET utf8mb4;
CREATE USER IF NOT EXISTS 'web_server'@'%' IDENTIFIED BY '123456';
GRANT ALL PRIVILEGES ON web_server.* TO 'web_server'@'%';
FLUSH PRIVILEGES;

-- 创建用户表
USE web_server;
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    password VARCHAR(100) NOT NULL,
    permission INT DEFAULT 1,
    avatar TEXT,
    login_count INT DEFAULT 0,
    last_login_time DATETIME,
    last_login_ip VARCHAR(45),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

### 构建与运行

```bash
# 克隆仓库（如果是独立使用）
git clone https://github.com/jyoushitou/cpp_MySQL.git
cd MySQL

# 构建
mkdir build && cd build
cmake ..
cmake --build .

# 运行（默认端口 60907）
./mysql_service

# 自定义参数
./mysql_service --port 60907 --host 0.0.0.0 --threads 4
```

---

## ⚙️ 连接池配置

连接池通过 `PoolConfig` 结构体配置：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `host` | localhost (Win) / 192.168.0.52 (Linux) | MySQL 主机地址 |
| `port` | 3306 | MySQL 端口 |
| `user` | web_server | 数据库用户 |
| `password` | 123456 | 数据库密码 |
| `database` | web_server | 默认数据库 |
| `minSize` | 2 | 最小连接数（常驻） |
| `maxSize` | 10 | 最大连接数 |
| `timeoutSeconds` | 30 | 获取连接超时时间 |
| `idleTimeoutSeconds` | 300 | 空闲连接超时（超过关闭） |
| `autoReconnect` | true | 是否自动重连 |

### 连接池健康检查

服务每 5 分钟自动执行一次健康检查，验证所有空闲连接的有效性，自动关闭失效连接并重建。

---

## 🔌 RPC 接口文档

> 服务地址：`ws://host:60907`，协议：JSON-RPC 2.0

### 用户服务

| 方法 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `user.login` | name, password, device_name, client_ip, user_agent | status, token, user_id, level, avatar, devices | 用户登录（支持多设备） |
| `user.register` | name, password | status, message | 用户注册 |
| `user.info` | token | user_id, name, level, device | 获取当前用户信息 |
| `user.logout` | token | status, message | 退出登录 |
| `user.devices` | token | devices[] | 获取设备列表 |
| `user.remove_device` | token, device_id | status, message | 移除设备 |

### 数据操作

| 方法 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `data.query` | token, sql | columns[], rows[] | 执行 SQL 查询 |
| `data.execute` | token, sql | affected_rows, insert_id | 执行 SQL 写入 |
| `data.ping` | (无) | pong, timestamp | 心跳检测 |

### 连接池管理

| 方法 | 参数 | 返回 | 说明 |
|------|------|------|------|
| `pool.status` | (无) | active, idle, total, config | 获取连接池状态 |
| `pool.resize` | max_size | status, new_max | 动态调整连接池大小 |

---

## 🔗 在 WebServer 架构中的位置

```
GRPCGateway (60906) ──调用──► MySQL RPC (60907)
                                 │
                                 ▼
                            MySQL 数据库
```

GRPCGateway 不再直连 MySQL，而是通过本服务的 RPC 接口进行数据库操作，实现数据库访问的集中管理。

---

## 📋 开发计划

- [ ] **gRPC 协议迁移** — 从 WebSocket JSON-RPC 迁移到标准 gRPC
- [ ] **读写分离** — 支持主从复制架构
- [ ] **慢查询日志** — 记录并告警慢 SQL
- [ ] **连接池动态伸缩** — 根据负载自动调整连接数
- [ ] **SQL 注入防护** — 参数化查询支持
- [ ] **集成 Prometheus 指标** — 连接池状态监控

---

## 📬 联系方式

- 仓库地址：[https://github.com/jyoushitou/cpp_MySQL.git](https://github.com/jyoushitou/cpp_MySQL.git)
- 父项目：[WebServer](https://github.com/jyoushitou/WebServer)
