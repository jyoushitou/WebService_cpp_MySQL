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

## ✨ 功能特性/TODO

- 🗄️ **连接池管理** — 支持最小/最大连接数、超时控制、空闲回收、健康检查
- 🔌 **WebSocket JSON-RPC** — 基于 Boost.Beast 的独立 RPC 服务
- 📦 **用户管理** — 用户注册、登录鉴权、信息查询、设备管理
- 🧵 **多线程 I/O** — 可配置线程数，支持大数据量的访问请求MySQL
- 🔄 **自动重连** — 连接池提供健康检查与自动恢复
- 📊 **池状态监控** — 活跃/空闲/总连接数实时查询
