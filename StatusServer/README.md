# StatusServer

FoolChat 的登录状态与负载均衡服务，gRPC 监听 `:50052`。在登录流程中为客户端挑选一台负载最低的 ChatServer，生成 uuid token 写入 Redis 并返回给 GateServer；同时承担 ChatServer 上线后对 token 的二次校验。

## 功能特性

- `StatusService.GetChatServer` — 按 Redis hash `logincount`（各 ChatServer 在线人数）选择负载最低的实例，生成 uuid token 写入 `utoken_<uid>`，返回 host / port / token
- `StatusService.Login` — 校验 uid + token 是否与 Redis 中一致（ChatServer 处理 TCP 登录时调用）
- 可选 ChatServer 列表由 `config.ini` 的 `[chatservers]` 段配置，各实例地址配在同名子段
- `ChatGrpcClient` — 到 ChatServer 的 gRPC 客户端（连接池），用于与消息服务交互
- `RedisMgr` / `MysqlDao` / `ConfigMgr` / `AsioIOServicePool` 等基础设施与其余服务同款

## 安装 / 依赖

- Windows + MSVC v143，x64
- Boost 1.81（Asio）、gRPC、hiredis、libjsoncpp、mysql-connector-c++
- 第三方依赖路径硬编码在 `PropertySheet.props`（`D:\cppsoft\...`）——**TODO：按本机路径修改后再构建**

构建：

```bash
msbuild StatusServer/StatusServer.sln /p:Configuration=Debug /p:Platform=x64
```

> 产物输出到 `x64/Debug/`，PostBuild 会拷贝 `config.ini` 与依赖 DLL。改了 `config.ini` 需重新构建或手动拷贝到输出目录。

### 配置

| 段 | 用途 |
|----|------|
| `[StatusServer]` | 本服务监听地址 |
| `[chatservers]` | 参与负载均衡的 ChatServer 名单 |
| `[chatserver1]` / `[chatserver2]` | 各实例的 host / port |
| `[Mysql]` / `[Redis]` | 数据库与缓存连接（`Passwd` 为占位符，部署时替换） |

## 使用方法

本服务没有对外 HTTP 接口，由 GateServer / ChatServer 通过 gRPC 调用，直接启动即可：

```bash
msbuild StatusServer/StatusServer.sln /p:Configuration=Debug /p:Platform=x64
.\x64\Debug\StatusServer.exe
```

验证登录主链路（需先起 VarifyServer、ChatServer、GateServer）：

```bash
# 登录成功后 GateServer 会返回 ChatServer 地址与 token
curl -X POST http://127.0.0.1:8080/user_login \
  -H "Content-Type: application/json" \
  -d '{"user":"alice","passwd":"your_password"}'
```

> gRPC 接口调试（`GetChatServer` / `Login` 的请求字段见 `message.proto`）——TODO：可自行用 grpcurl 或 Postman gRPC 客户端联调。

## 目录结构

```text
StatusServer/
├── StatusServer.cpp          # 入口（main）
├── StatusServiceImpl.*       # StatusService gRPC 服务实现
├── ChatGrpcClient.*          # ChatServer gRPC 客户端（连接池）
├── MysqlDao.* / MysqlMgr.*   # MySQL 访问
├── RedisMgr.*                # Redis 访问（logincount / utoken）
├── ConfigMgr.*               # config.ini 读取
├── AsioIOServicePool.*       # io_context 线程池
├── Singleton.h / const.h     # 单例模板与常量 / 错误码
├── message.proto             # gRPC 定义（含 StatusService / ChatService）
├── message.pb.* / message.grpc.pb.*   # protoc 生成代码
├── PropertySheet.props       # 第三方依赖路径（按机器修改）
├── config.ini                # 运行配置（占位符）
├── StatusServer.sln          # MSVC 解决方案
└── StatusServer.vcxproj      # MSVC 工程
```
