# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

FoolChat 分布式即时通讯系统。服务端为 Boost.Asio + gRPC（MSVC x64），客户端为 Qt6，验证码服务为 Node.js。根目录 `mhkh.sql` 是 MySQL schema 导出（库名 `mhkh`）。

服务目录：

- `GateServer/` — HTTP 网关（Boost.Beast，:8080），接收客户端 HTTP 请求并转发到 gRPC 服务
- `VarifyServer/` — Node.js gRPC 服务（:50051），派发邮箱验证码（nodemailer + Redis）
- `StatusServer/` — gRPC 服务（:50052），登录状态与负载均衡，为登录用户分配 ChatServer 并签发 token
- `ChatServer/`、`ChatServer2/` — TCP 实时消息服务器两份实例（TCP :8090/:8091，gRPC :50055/:50056），代码几乎相同，靠 `config.ini` 的 `SelfServer` 段区分身份
- `Chat/` — Qt6 桌面客户端（qmake，Fluent Design / ElaWidgetTools UI）
- `fool_chat_admin_go/` — Go 后台管理系统（HTTP :9100），管理用户/动态/公告/通知，带 AI 助手；有独立的 `fool_chat_admin_go/.claude/CLAUDE.md`，进该目录先读它

## 构建与运行

无测试、无 lint 配置。依赖路径硬编码在各 C++ 服务的 `PropertySheet.props`（`D:\cppsoft\...`：boost_1_81_0、grpc、hiredis、libjson、mysql-connector-c++），换机器需先改这里。

### C++ 服务（MSVC v143，x64 Debug）

```bash
msbuild GateServer\GateServer.sln /p:Configuration=Debug /p:Platform=x64
msbuild StatusServer\StatusServer.sln /p:Configuration=Debug /p:Platform=x64
msbuild ChatServer\ChatServer.sln /p:Configuration=Debug /p:Platform=x64
msbuild ChatServer2\ChatServer.sln /p:Configuration=Debug /p:Platform=x64
```

产物在各自 `x64/Debug/`。PostBuild 会把 `config.ini` 和 DLL 拷到输出目录——改了 `config.ini` 需重新构建或手动拷贝。

### 修改 message.proto 后重新生成代码

`start.bat` 不是启动脚本，是 protoc 生成脚本（路径硬编码 `D:\cppsoft\grpc\...`），只在 ChatServer / ChatServer2 下有：

```bash
cd ChatServer\ChatServer && start.bat   # ChatServer2 同理
```

GateServer / StatusServer 没有现成脚本，改 proto 后需手动跑同样的两条 protoc 命令（`--grpc_out` + `--cpp_out`），或从 ChatServer 拷一份改。

### Qt 客户端（Chat/）

**只用 MSVC kit 构建**（Qt 6.8.3 MSVC2022 64bit，Qt Creator + jom）。2026-09 已从 MinGW 切到 MSVC：

- ElaWidgetTools 依赖：`D:/cppsoft/MsvcEla`（MSVC 版，路径写死在 `Chat.pro`；MinGW 版 `RabbitEla` 已弃用）
- `MsvcEla` 的 DLL 是 **Release 编译**的 → **Debug kit 启动必崩**（Ela connect nullptr + signal 11），跑 Release 构建；要 Debug 得先用 Debug 重编 Ela
- `QT += webenginewidgets`（后台页内嵌需要）→ 换 `QT +=` 后重新构建让 Creator 重跑部署，或手动 `windeployqt bin\Chat.exe`，否则缺 `QtWebEngineProcess.exe` 会闪退
- 构建后 PostBuild 自动拷贝 `config.ini`、`static/`、`ElaWidgetTools.dll` 到 `bin/`
- 运行时读的是 `bin/config.ini`（`GateServer/host`、`port`），不是源码目录的那份

### Node 验证码服务

```bash
cd VarifyServer
npm run serve    # = node server.js，绑定 0.0.0.0:50051
```

密钥/连接配置在 `VarifyServer/config.json`（邮箱密码、MySQL、Redis）——**不要提交、不要打印到日志**。

### 推荐启动顺序

MySQL(服务器)  + Redis (服务器) → VarifyServer(服务器) → StatusServer → ChatServer(s) → GateServer → Chat 客户端。（后台管理 `fool_chat_admin_go` 可选，依赖 MySQL + Redis + VarifyServer，`go run .` 默认 :9100）

## 架构

### 登录流程（跨服务主链路）

```
Qt Client ──HTTP POST /user_login──▶ GateServer
  GateServer ──查 MySQL 校验密码──▶ 成功后
  GateServer ──gRPC GetChatServer──▶ StatusServer
    StatusServer 按 Redis hash logincount 选负载最低的 ChatServer，
    生成 uuid token 写入 Redis，返回 host/port/token
  GateServer 把 uid/token/host/port 回给客户端
Qt Client ──TCP 连 ChatServer，发 1005 CHAT_LOGIN（uid+token）──▶
  ChatServer 验 token，绑定 session，写 Redis uip_<uid>=本服名
```

### HTTP 端点（GateServer/LogicSystem.cpp 构造函数里注册）

| 路由 | 作用 |
|------|------|
| `POST /get_varifycode` | 转发 gRPC 到 VarifyServer 取邮箱验证码 |
| `POST /user_register` | 比对 Redis `code_<email>`（VarifyServer 写入，600s 有效）后调 MySQL 存储过程 `reg_user` 注册 |
| `POST /reset_pwd` | 重置密码 |
| `POST /user_login` | 见上方登录流程 |
| `GET /get_test` | 测试用 |

### 跨 ChatServer 实例转发

- 消息路由靠 Redis：`uip_<uid>` → 目标服务器名（登录时写入）
- 目标不在本机 → `ChatGrpcClient`（按服务器名维护连接池，池大小/地址来自 `config.ini` 的 `[PeerServer]` 段）调用对方 `ChatService` gRPC（NotifyAddFriend / NotifyAuthFriend / NotifyTextChatMsg / NotifyKickUser），对端 `ChatServiceImpl` 查本地 session 后下发 TCP
- StatusServer 各实例的可选服务器列表来自 `config.ini` `[chatservers]` 段
- ChatServer和ChatServer2需要同步文件改动，但是不需要同步config.ini配置

### TCP 协议（客户端 ↔ ChatServer）

4 字节头（2 字节 msg_id + 2 字节 body 长度）+ JSON body。`MAX_LENGTH = 2KB`。

消息 ID 双端必须同步：

- 服务端：`ChatServer*/const.h` 的 `MSG_IDS`（1005 起为 TCP 消息；1001–1004 是 HTTP 业务错误码域）
- 客户端：`Chat/src/core/global.h` 的 `ReqId`（同一套编号）
- 业务错误码：各服务 `const.h` 的 `ErrorCodes`（服务端 1001 起，客户端 `global.h` 0/1/2——**两端 ErrorCodes 不一致，回包判断时注意**

改协议时在两处同步加 ID，并在 `LogicSystem::RegisterCallBacks`（服务端）和 `TcpMgr::initHandlers`（客户端）注册处理函数。

### 服务端通用模式

- `Singleton<T>` CRTP 单例，几乎所有 Manager 都挂在这上面
- `LogicSystem`：HTTP 路由表（GateServer）/ TCP 回调表（ChatServer），构造时集中注册
- `AsioIOServicePool`：round-robin 的 `io_context` 线程池（数量 = CPU 核数），连接级负载分摊
- `ConfigMgr`：读 `config.ini`（Boost.PropertyTree），各服务都有自己的副本
- gRPC client 全部走连接池（`ChatConPool` / Status / Varify 同款模式）

### 共享文件不同步风险

以下文件在多个目录有**独立副本**，改动需手动同步：

- `message.proto`：4 个 C++ 服务内完全相同；`VarifyServer/message.proto` 是子集（仅 VarifyService，且 `GetVarifyReq` 没有 `username` 字段）——扩展公共 message 时两边都要改
- `const.h`：各服务副本**内容已分叉**（ErrorCodes 字段不完全一致），改公共枚举前先 diff 各份
- `ConfigMgr.*`、`RedisMgr.*`、`MysqlDao.*`、`Singleton.h` 等基础设施代码是复制粘贴维护的，无共享库

### MySQL / Redis

- Schema `mhkh`（根目录 `mhkh.sql`）：`user`、`friend`、`friend_apply`、`dynamic`、`user_id`（发号器）、`StarNotice`；注册走存储过程 `reg_user`
- Redis key 前缀（定义在 `const.h`）：`code_` 验证码、`uip_` 用户所在服务器、`utoken_` 登录 token、`ubaseinfo_` 用户缓存、`usession_`、`logincount`（在线数 hash，StatusServer 负载均衡依据）
- 分布式锁：`lock_` 前缀，超时 10s / 获取 5s

### 客户端（Chat/）模块

- `src/core/` — HttpMgr（ReqId+Modules 分发）、TcpMgr（handler map）、DBManager、UserMgr、Logger（宏 `LOG_DEBUG/INFO/WARN/ERROR`）
- `src/ui/` — 登录/注册/重置（QStackedWidget 切页）
- `Chat/Admin_Page/` — 后台管理页：`QWebEngineView` 内嵌管理端网页，地址硬编码在 `adminwid.cpp` 的 `kAdminUrl`（`http://127.0.0.1:9100/`，可通过 QSettings `admin/url` 覆盖）
- `Chat/` — 主窗口与各功能页（聊天、通讯录、动态、音乐等），`Chat/Chat_Comp/` 是可复用组件与数据结构（`userdata.h`）
- 单例用 `F_Singleton`（`src/core/F_singleton.h`，与服务端 `Singleton.h` 逻辑相同但类名不同，各自独立副本）

## Formatting

- 单行 `if`、`else`、`for`、`while` 也必须使用大括号
- `switch` 中的 `case`、`default` 分支体必须显式添加大括号
- 函数定义必须展开，函数签名和大括号分别换行
- 使用项目根目录的 `.clang-format` 进行格式化（LLVM-based, 4-space indent, Allman braces）

## Comments

- 代码标识符（类名、方法名、变量名）：英文
- 文档注释和用户可见信息：中文
- 不要写解释代码做什么的注释 — 起好名字就够了。只在 WHY 不明显时才加注释（隐藏的约束、微妙的不变量、变通方案）
- 内部结构体的每个字段必须有注释，说明数据身份、业务含义或有效条件

## Casting

仅使用 C++ 风格的类型转换：`static_cast<T>(val)`, `dynamic_cast<T*>(ptr)`, `reinterpret_cast<T>(ptr)`。禁止 C 风格转换。

### 控件命名

控件变量使用"业务含义 + 组件类型"命名，末尾体现组件类型：

- 好：`_unitComboBox`, `_minimumEdit`, `_applyPushButton`
- 不好：`_unit`, `_minimum`

View 成员同时说明业务用途与实际类型，如 `_parameterTableView`、`_formulaTreeView`。

Model 成员按业务职责命名，如 `_parameterInstantiationModel`、`_formulaOptionModel`。

布局变量命名为 `mainLayout`，子布局按职责命名并以 `Layout` 结尾，如 `buttonLayout`。

### QString 字面量

不要使用 `QStringLiteral`：

```cpp
QString text = "普通字符串";              // 好
QString text = QStringLiteral("普通字符串");  // 不好

text += QString("格式：%1").arg(value);   // 好
```

