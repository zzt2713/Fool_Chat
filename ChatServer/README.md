# ChatServer

FoolChat 的 TCP 实时消息服务（实例 1），TCP 监听 `:8090`，gRPC 监听 `:50055`。负责登录会话管理、实时消息收发与跨服务器转发；与 [ChatServer2](../ChatServer2/) 代码几乎完全相同，靠 `config.ini` 的 `[SelfServer]` 段区分身份。

## 功能特性

- TCP 应用层协议：**4 字节头（2 字节 msg_id + 2 字节 body 长度，网络字节序）+ JSON body**，单条上限 2KB
- `CHAT_LOGIN`（msg_id `1005`）：客户端携带 uid + token 登录，先比对 Redis `utoken_<uid>`，再经 gRPC `StatusService.Login` 二次校验，成功后绑定 session 并写入 `uip_<uid>`（记录用户所在服务器）
- `LogicSystem::RegisterCallBacks` — TCP 消息回调表，按 msg_id 分发处理
- 跨服转发：目标用户不在本机时，经 `ChatGrpcClient` 调对端 `ChatService` gRPC（`NotifyAddFriend` / `NotifyAuthFriend` / `NotifyTextChatMsg` / `NotifyKickUser`），对端查本地 session 后下发 TCP
- `CSession` 会话管理与发送队列，`AsioIOServicePool` 多线程 IO 分摊

### 消息 ID（节选）

| msg_id | 含义 |
|--------|------|
| `1005` | `MSG_CHAT_LOGIN` 登录请求（body：`{"uid":10001,"token":"..."}`） |
| `1006` | `MSG_CHAT_LOGIN_RSP` 登录回包 |
| `1007` | `ID_SEARCH_USER_REQ` 搜索用户 |

完整列表见 `const.h` 的 `MSG_IDS`；客户端侧对应 `src/core/global.h` 的 `ReqId`，**两端必须同步修改**。

## 安装 / 依赖

- Windows + MSVC v143，x64
- Boost 1.81（Asio）、gRPC、hiredis、libjsoncpp、mysql-connector-c++
- 第三方依赖路径硬编码在 `PropertySheet.props`（`D:\cppsoft\...`）——**TODO：按本机路径修改后再构建**

构建：

```bash
msbuild ChatServer/ChatServer.sln /p:Configuration=Debug /p:Platform=x64
```

> 产物输出到 `x64/Debug/`，PostBuild 会拷贝 `config.ini` 与依赖 DLL。

### 配置

`config.ini` 关键段：

| 段 | 用途 |
|----|------|
| `[SelfServer]` | 本实例身份：Name / TCP 端口 / gRPC 端口 |
| `[PeerServer]` / `[chatserver2]` | 跨服转发的对端 gRPC 地址 |
| `[Mysql]` / `[Redis]` | 数据库与缓存连接（`Passwd` 为占位符，部署时替换） |

### 重新生成 gRPC 代码

修改 `message.proto` 后，在本目录执行（`start.bat` 是 protoc 生成脚本，**不是启动脚本**，内含本机硬编码的 protoc 路径——**TODO：按本机路径修改**）：

```bat
start.bat
```

## 使用方法

1. 启动依赖：MySQL、Redis、StatusServer
2. 构建并运行 `x64/Debug/ChatServer.exe`
3. 客户端先通过 GateServer `/user_login` 拿到 uid / token，再建立 TCP 连接发送登录帧：

```python
import json
import socket
import struct

body = json.dumps({"uid": 10001, "token": "<登录返回的 token>"}).encode()
header = struct.pack(">HH", 1005, len(body))  # >HH = 网络字节序的 msg_id + 长度

s = socket.create_connection(("127.0.0.1", 8090))
s.sendall(header + body)
print(s.recv(2048))
```

## 目录结构

```text
ChatServer/
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
├── config.ini                # 运行配置（占位符）
├── ChatServer.sln            # MSVC 解决方案
└── ChatServer.vcxproj        # MSVC 工程
```
