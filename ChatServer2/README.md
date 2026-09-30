# ChatServer2

FoolChat 的 TCP 实时消息服务（实例 2），TCP 监听 `:8091`，gRPC 监听 `:50056`。代码与 [ChatServer](../ChatServer/) 几乎完全相同，两份实例通过 `config.ini` 的 `[SelfServer]` 段区分身份，用于演示分布式部署与跨服消息转发。

## 功能特性

- 与 ChatServer 完全一致的 TCP 协议与登录流程：**4 字节头（2 字节 msg_id + 2 字节 body 长度，网络字节序）+ JSON body**，单条上限 2KB
- `CHAT_LOGIN`（msg_id `1005`）：uid + token 经 Redis 与 StatusServer 双重校验后绑定 session
- 跨服转发：本机不持有目标用户时，经 `ChatGrpcClient` 调对端 `ChatService` gRPC 转发（好友申请、聊天消息、踢人等）
- 靠 `config.ini` 区分身份：
  - `[SelfServer]` → `Name = chatserver2`，TCP `8091`，RPC `50056`
  - `[PeerServer]` → 对端 `chatserver1`（gRPC `50055`）

### 与 ChatServer 的同步约定

> 两份实例的**源码改动必须双向同步**，但 `config.ini` 各自维护、**不**同步。

## 安装 / 依赖

- Windows + MSVC v143，x64
- Boost 1.81（Asio）、gRPC、hiredis、libjsoncpp、mysql-connector-c++
- 第三方依赖路径硬编码在 `PropertySheet.props`（`D:\cppsoft\...`）——**TODO：按本机路径修改后再构建**

构建：

```bash
msbuild ChatServer2/ChatServer.sln /p:Configuration=Debug /p:Platform=x64
```

> 产物输出到 `x64/Debug/`，PostBuild 会拷贝 `config.ini` 与依赖 DLL。

### 配置

| 段 | 用途 |
|----|------|
| `[SelfServer]` | 本实例身份：`chatserver2`，TCP `8091` / gRPC `50056` |
| `[PeerServer]` / `[chatserver1]` | 对端 ChatServer1 的 gRPC 地址 |
| `[Mysql]` / `[Redis]` | 数据库与缓存连接（`Passwd` 为占位符，部署时替换） |

### 重新生成 gRPC 代码

修改 `message.proto` 后执行本目录的 `start.bat`（protoc 生成脚本，**不是启动脚本**，内含本机硬编码的 protoc 路径——**TODO：按本机路径修改**）。

## 使用方法

1. 启动依赖：MySQL、Redis、StatusServer，以及对端 ChatServer
2. 构建并运行 `x64/Debug/ChatServer.exe`（注意与实例 1 的输出目录分开，避免互相覆盖）
3. 客户端登录拿到 uid / token 后连接 `127.0.0.1:8091`：

```python
import json
import socket
import struct

body = json.dumps({"uid": 10001, "token": "<登录返回的 token>"}).encode()
header = struct.pack(">HH", 1005, len(body))  # >HH = 网络字节序的 msg_id + 长度

s = socket.create_connection(("127.0.0.1", 8091))
s.sendall(header + body)
print(s.recv(2048))
```

分配逻辑：StatusServer 会按 Redis `logincount` 把新登录用户分配到在线人数较少的实例——两个实例都启动后即可观察跨服转发。

## 目录结构

```text
ChatServer2/
├── ChatServer.cpp            # 入口（main）
├── CServer.*                 # TCP 服务
├── CSession.*                # 连接会话、收发缓冲与登录流程
├── LogicSystem.*             # TCP 消息回调注册与业务处理
├── MsgNode.* / data.h        # 收发消息节点与数据结构
├── ChatServiceImpl.*         # gRPC ChatService 实现（接收跨服转发）
├── ChatGrpcClient.*          # 对端 ChatServer gRPC 客户端（连接池）
├── StatusGrpcClient.*        # StatusServer gRPC 客户端（token 校验）
├── UserMgr.*                 # 在线用户管理
├── MysqlDao.* / MysqlMgr.* / RedisMgr.*   # 存储访问
├── ConfigMgr.* / Singleton.h / const.h    # 配置、单例、常量与消息 ID
├── AsioIOServicePool.*       # io_context 线程池
├── message.proto             # gRPC 定义
├── message.pb.* / message.grpc.pb.*   # protoc 生成代码
├── start.bat                 # protoc 重新生成脚本（非启动脚本）
├── PropertySheet.props       # 第三方依赖路径（按机器修改）
├── config.ini                # 运行配置（占位符，不与 ChatServer 同步）
├── ChatServer.sln            # MSVC 解决方案
└── ChatServer.vcxproj        # MSVC 工程
```
