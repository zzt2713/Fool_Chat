# Fool_Chat_Client

FoolChat 的 Qt6 桌面客户端，Fluent Design 风格（ElaWidgetTools）。覆盖账号体系（注册 / 登录 / 重置密码，邮箱验证码）、即时聊天、通讯录与好友申请、动态、音乐、公告通知，并内置内嵌式后台管理页（`QWebEngineView` 加载 Web 管理端）。

## 功能特性

- **账号**：登录 / 注册 / 重置密码三页切换（`QStackedWidget`），邮箱验证码接入
- **聊天**：会话列表、气泡消息（文字 / 图片）、搜索，TCP 长连接实时收发（`TcpMgr`）
- **通讯录**：好友列表、好友申请与验证、好友资料页
- **动态**：朋友圈式动态发布与浏览
- **音乐 / 公告通知 / 编辑器 / 设置**：独立功能页
- **后台管理页**：`QWebEngineView` 内嵌管理端网页，地址读 `config.ini` 的 `[admin] url`（默认 `http://127.0.0.1:9100/`）
- **AI 助手**：`src/core/aimgr` 封装的 AI 能力
- **核心模块**（`src/core/`）：`HttpMgr`（HTTP 请求 + ReqId 分发）、`TcpMgr`（TCP handler map）、`DBManager`、`UserMgr`、`Logger`（`LOG_DEBUG/INFO/WARN/ERROR`）

## 安装 / 依赖

- Qt **6.8.3 MSVC2022 64bit**（qmake 构建，**仅 MSVC kit**，不支持 MinGW）
- ElaWidgetTools（Fluent UI 库）：路径硬编码为 `D:/cppsoft/RabbitEla`——**TODO：按本机路径修改 `Chat.pro`**
- Qt 模块：`core gui network sql multimedia multimediawidgets widgets`，另需 **`webenginewidgets`**（后台管理页用到 `QWebEngineView`）

构建：

```bash
# 推荐：Qt Creator 打开 Chat.pro，选择 MSVC kit，Release 构建
# 或命令行：
qmake Chat.pro
nmake release      # 或 jom
```

### 配置

运行时读取 `bin/config.ini`：

```ini
[GateServer]
host = localhost      # GateServer 地址
port = 8080

[admin]
url = http://YOUR_SERVER_IP:9100/   # 后台管理页地址

[ai]
base_url = https://api.deepseek.com
model = deepseek-v4-flash
api_key = YOUR_API_KEY            
```

## 使用方法

1. 启动服务端（MySQL + Redis → VarifyServer → StatusServer → ChatServer(s) → GateServer）
2. 运行 `bin/Chat.exe`，在登录页切换到注册页，用邮箱验证码注册账号
3. 登录后进入主窗口：左侧切换聊天 / 通讯录 / 动态 / 音乐 / 公告等页面
4. 「后台管理」页会内嵌加载 `[admin] url` 指向的 Web 管理端（需 [fool_chat_admin_go](../fool_chat_admin_go/) 在运行）

TCP 登录由客户端自动完成：GateServer `/user_login` 拿到 uid / token 后，`TcpMgr` 连接 ChatServer 发送 `1005 CHAT_LOGIN`。

## 目录结构

```text
Fool_Chat_Client/
├── Chat.pro                # qmake 工程（主构建方式）
├── CMakeLists.txt          # CMake 备用（TODO：未验证）
├── config.ini              # 运行配置（占位符）
├── src/                    # 入口与核心模块
│   ├── main.cpp
│   ├── core/               # HttpMgr / TcpMgr / DBManager / UserMgr / Logger / aimgr / global
│   ├── ui/                 # 登录 / 注册 / 重置密码 / 主窗口
│   └── widgets/            # 通用控件（提示、动效、按钮等）
├── Chat/                   # 主窗口与各功能页
│   ├── c_window.*          # 主窗口
│   ├── Chat_Page/          # 聊天页
│   ├── Contact_Page/       # 通讯录页
│   ├── Dynamic_Page/       # 动态页
│   ├── Music_Page/         # 音乐页
│   ├── Notice_Page/        # 公告通知页
│   ├── Editor_Page/        # 编辑器页
│   ├── Setting_Page/       # 设置页
│   ├── About_Page/         # 关于页
│   ├── Admin_Page/         # 后台管理页（WebEngine 内嵌）
│   └── Chat_Comp/          # 可复用组件与数据结构（userdata 等）
├── icons/                  # 图标资源
└── style/                  # 样式资源
```
