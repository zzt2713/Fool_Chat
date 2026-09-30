# fool_chat_admin_go

FoolChat 的 Web 后台管理端，Go 实现，默认监听 `0.0.0.0:9100`。提供用户、动态、公告、通知、管理员申请审核、操作日志、数据概览、服务监控、数据备份导出与 AI 助手等能力，前端为单页管理界面（`templates/index.html` + `static/`）。

## 功能特性

- **登录鉴权**：`POST /api/login`，按 `user` 表校验账号密码与管理角色，会话 token 保持登录态
- **用户管理**：用户查询与运营操作（高危操作需二级密码）
- **动态 / 公告 / 通知**：内容管理与发布
- **邮件通知**：SMTP 邮件发送（`mail.go`）
- **管理员申请**：用户提交申请、管理员审核，支持 AI 批量驳回
- **操作日志**：登录、审核、导出等操作全部落 `admin_operation_log`
- **数据概览**：统计接口（`handler_stats`）
- **服务监控**：对 Gate / Status / ChatServer / VarifyServer 探活，`monitor_overrides.json` 可覆盖目标地址
- **数据备份**：按类型导出 CSV，或打包为 `fool_chat_backup_csv.zip` 全量导出
- **AI 助手**：对接 DeepSeek 兼容接口
- **部署**：附带 systemd 单元 `fool-chat-admin.service`

## 安装 / 依赖

- Go 1.25+（`go.mod` 声明 `go 1.25.0`）
- MySQL（`mhkh` 库）、Redis；邮件 / 验证码相关功能依赖 [VarifyServer](../VarifyServer/)
- 主要依赖：`github.com/go-sql-driver/mysql`、`github.com/redis/go-redis/v9`、`google.golang.org/grpc`

```bash
cd fool_chat_admin_go

# 1. 准备配置（config.yaml 不在仓库内，敏感信息勿提交）
cp config.example.yaml config.yaml
# 编辑 config.yaml：数据库密码、AI api_key、SMTP、Redis 等填真实值

# 2. 安装依赖并启动
go mod download
go run .
```

### 配置

`config.yaml` 字段见 [config.example.yaml](./config.example.yaml)，要点：

| 段 | 说明 |
|----|------|
| `app.addr` | 监听地址，默认 `0.0.0.0:9100` |
| `database.*` | MySQL 连接（`password` 占位，部署时替换） |
| `security.delete_password` | 删除用户等高危操作的二级密码 |
| `ai.*` | AI 服务地址 / api_key / 模型 |
| `redis.*` / `verify_server.*` | 验证码相关依赖 |
| `services.*` | 分布式 IM 各服务的探活地址 |
| `smtp.*` | 管理员申请通知邮件配置 |

服务地址也可由 [monitor_overrides.json](./monitor_overrides.json) 覆盖（当前为占位符）。配置缺失时按内置默认值（`127.0.0.1` 系列）运行。

## 使用方法

```bash
go run .           # 或 go build -o fool_chat_admin_go . 后运行
```

浏览器打开管理端：

```text
http://127.0.0.1:9100/
```

用 `user` 表中具备管理角色的账号登录。API 示例：

```bash
# 登录（成功返回会话 token）
curl -X POST http://127.0.0.1:9100/api/login \
  -H "Content-Type: application/json" \
  -d '{"name":"your_admin_name","password":"your_password"}'
```

Linux 部署（systemd）：

```bash
# 将构建产物与配置放到 /opt/fool_chat_admin_go 后
sudo cp fool-chat-admin.service /etc/systemd/system/
sudo systemctl enable --now fool-chat-admin
```

## 目录结构

```text
fool_chat_admin_go/
├── main.go                  # 入口：路由注册、自动建表
├── config.go                # 配置加载（读取 config.yaml，缺失时用默认值）
├── config.example.yaml      # 配置模板（占位符）
├── auth.go                  # 登录 / 会话鉴权
├── db.go / log.go / util.go / mail.go   # 数据库、日志、工具、SMTP
├── verify_client.go         # VarifyServer gRPC 客户端
├── handler_user.go          # 用户管理
├── handler_dynamic.go       # 动态管理
├── handler_notice.go        # 公告 / 通知
├── handler_email.go         # 邮件通知
├── handler_admin_apply.go   # 管理员申请审核
├── handler_password_reset.go# 密码重置
├── handler_stats.go         # 数据概览
├── handler_monitor.go       # 服务监控探活
├── handler_maintenance.go   # 数据备份导出（CSV / ZIP）
├── handler_ai.go            # AI 助手
├── monitor_overrides.json   # 探活地址覆盖（占位符）
├── templates/index.html     # 管理端单页
├── static/                  # 前端资源（app.js / app.css / xlsx 等）
├── fool-chat-admin.service  # systemd 部署单元
└── go.mod / go.sum
```
