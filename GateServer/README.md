# GateServer

FoolChat 的 HTTP 网关服务，基于 Boost.Beast，默认监听 `:8080`。负责接收客户端的注册、登录、重置密码与验证码请求：校验 MySQL 账号密码、比对 Redis 中的邮箱验证码，并在登录成功后调用 StatusServer 获取分配的 ChatServer 地址与登录 token，一并返回给客户端。

## 功能特性

- HTTP 路由（构造时在 `LogicSystem` 中集中注册）：
  - `POST /get_varifycode` — 转发 gRPC 到 VarifyServer 获取邮箱验证码
  - `POST /user_register` — 比对 Redis 验证码后调 MySQL 存储过程 `reg_user` 注册
  - `POST /reset_pwd` — 重置密码
  - `POST /user_login` — 登录：MySQL 校验密码 → gRPC `GetChatServer` 向 StatusServer 申请 ChatServer 与 token
  - `GET /get_test` — 测试端点
- gRPC 客户端连接池（VarifyServer / StatusServer），连接参数来自 `config.ini`
- `AsioIOServicePool`：round-robin 的 `io_context` 线程池，按连接分摊负载
- `RedisMgr` / `MysqlDao`：Redis 与 MySQL 访问封装，`Singleton<T>` 单例模式

## 安装 / 依赖

- Windows + MSVC v143，x64
- Boost 1.81（Beast / Asio）、gRPC、hiredis、libjsoncpp、mysql-connector-c++
- 第三方依赖路径硬编码在 `PropertySheet.props`（`D:\cppsoft\...`）——**TODO：按本机路径修改后再构建**

构建：

```bash
msbuild GateServer/GateServer.sln /p:Configuration=Debug /p:Platform=x64
```

> 产物输出到 `x64/Debug/`，PostBuild 会自动拷贝 `config.ini` 与依赖 DLL。

### 配置

运行时读取 `config.ini`（与可执行文件同目录）：

| 段 | 用途 |
|----|------|
| `[GateServer]` | 网关监听端口 |
| `[VarifyServer]` / `[StatusServer]` | 下游 gRPC 服务地址 |
| `[Mysql]` / `[Redis]` | 数据库与缓存连接（`Passwd` 为占位符，部署时替换） |
| `[chatservers]` / `[PeerServer]` | ChatServer 实例列表与对端地址 |

## 使用方法

1. 启动依赖：MySQL、Redis、VarifyServer、StatusServer、ChatServer(s)
2. 构建并运行 `x64/Debug/GateServer.exe`
3. 调用 HTTP 接口（响应 JSON 的 `error` 字段为 `0` 表示成功）：

```bash
# 获取邮箱验证码
curl -X POST http://127.0.0.1:8080/get_varifycode \
  -H "Content-Type: application/json" \
  -d '{"email":"your-email@example.com"}'

# 注册（varifycode 为邮箱收到的 4 位验证码）
curl -X POST http://127.0.0.1:8080/user_register \
  -H "Content-Type: application/json" \
  -d '{"email":"your-email@example.com","user":"alice","passwd":"your_password","confirm":"your_password","varifycode":"1234"}'

# 登录（成功后返回 uid、token、ChatServer host/port，客户端凭此建立 TCP 连接）
curl -X POST http://127.0.0.1:8080/user_login \
  -H "Content-Type: application/json" \
  -d '{"user":"alice","passwd":"your_password"}'

# 重置密码
curl -X POST http://127.0.0.1:8080/reset_pwd \
  -H "Content-Type: application/json" \
  -d '{"email":"your-email@example.com","user":"alice","passwd":"new_password","varifycode":"1234"}'
```

## 目录结构

```text
GateServer/
├── GateServer.cpp            # 入口（main）
├── CServer.* / HttpConnection.*   # HTTP 服务与连接管理
├── LogicSystem.*             # 路由注册与各端点处理逻辑
├── StatusGrpcClient.*        # StatusServer gRPC 客户端（连接池）
├── VarifyGrpcClient.*        # VarifyServer gRPC 客户端（连接池）
├── MysqlDao.* / MysqlMgr.*   # MySQL 访问
├── RedisMgr.*                # Redis 访问
├── ConfigMgr.*               # config.ini 读取
├── AsioIOServicePool.*       # io_context 线程池
├── Singleton.h / const.h     # 单例模板与常量 / 错误码
├── message.proto             # gRPC 定义
├── message.pb.* / message.grpc.pb.*   # protoc 生成代码
├── PropertySheet.props       # 第三方依赖路径（按机器修改）
├── config.ini                # 运行配置（占位符）
├── GateServer.sln            # MSVC 解决方案
└── GateServer.vcxproj        # MSVC 工程
```
