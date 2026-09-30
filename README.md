# FoolChat — 分布式即时通讯系统

FoolChat 是一套完整的分布式即时通讯系统项目集合：C++ 服务端（Boost.Asio + gRPC，MSVC x64）、Node.js 验证码服务、Go Web 后台管理端、Qt6 桌面客户端。各服务通过 gRPC、Redis 与 MySQL 协作，完成注册、登录、负载均衡、实时消息与跨服务器转发。

## 项目列表

| 项目 | 说明 |
|------|------|
| [GateServer](./GateServer/) | HTTP 网关（Boost.Beast，:8080）。接收客户端的注册 / 登录 / 重置密码 / 验证码请求，校验 MySQL 后联动 StatusServer 完成登录，下发 uid、token 与 ChatServer 地址 |
| [VarifyServer](./VarifyServer/) | Node.js gRPC 验证码服务（:50051）。生成邮箱验证码，经 SMTP 发送，并写入 Redis（600 秒有效） |
| [StatusServer](./StatusServer/) | gRPC 状态服务（:50052）。按 Redis 在线人数为登录用户分配负载最低的 ChatServer，并签发登录 token |
| [ChatServer](./ChatServer/) | TCP 实时消息服务实例 1（TCP :8090 / gRPC :50055）。会话管理、消息路由与跨服转发 |
| [ChatServer2](./ChatServer2/) | TCP 实时消息服务实例 2（TCP :8091 / gRPC :50056）。与实例 1 共享代码，靠 `config.ini` 的 `[SelfServer]` 段区分身份 |
| [Fool_Chat_Client](./Fool_Chat_Client/) | Qt6 桌面客户端（Fluent Design / ElaWidgetTools）。聊天、通讯录、动态、音乐、公告，以及内嵌的后台管理页 |
| [fool_chat_admin_go](./fool_chat_admin_go/) | Go Web 后台管理端（:9100）。管理用户、动态、公告、通知、管理员申请，带服务监控、数据备份与 AI 助手 |

## 目录结构

```text
.
├── README.md               # 本文件：项目集合总览
├── CLAUDE.md               # 开发指引（架构、协议、构建说明）
├── .gitignore
├── GateServer/             # HTTP 网关服务
├── VarifyServer/           # 邮箱验证码服务（Node.js gRPC）
├── StatusServer/           # 登录状态与负载均衡服务（gRPC）
├── ChatServer/             # TCP 实时消息服务实例 1
├── ChatServer2/            # TCP 实时消息服务实例 2
├── Fool_Chat_Client/       # Qt6 桌面客户端
├── start_services.bat		   # 服务器启动脚本
└── fool_chat_admin_go/     # Go 后台管理端

```

## 启动顺序

```text
MySQL + Redis → VarifyServer → StatusServer → ChatServer / ChatServer2 → GateServer → 客户端 / 后台管理
```

各项目的依赖、构建、配置与使用示例见对应目录下的 README。
