# VarifyServer

FoolChat 的邮箱验证码服务，Node.js 实现的 gRPC 服务，绑定 `0.0.0.0:50051`。为注册 / 重置密码流程生成 4 位大写验证码：通过 SMTP 发送邮件，并写入 Redis（`code_<email>`，600 秒有效）。GateServer 通过 gRPC `GetVarifyCode` 调用本服务。

## 功能特性

- `VarifyService.GetVarifyCode(email)` — 生成 4 位大写验证码（uuid 截取），Redis 600 秒过期；已有未过期验证码时直接复用
- nodemailer 经 163 邮箱 SMTP 发送 HTML 验证码邮件
- 配置外置 `config.json`：邮箱账号、MySQL、Redis 连接（均为占位符）
- `message.proto` 是公共 proto 的子集（仅 `VarifyService`；`GetVarifyReq` 无 `username` 字段）——扩展公共 message 时需与 C++ 服务侧的 proto 同步

## 安装 / 依赖

- Node.js（建议 LTS；`package.json` 未锁定 engines 版本）
- 依赖：`@grpc/grpc-js`、`@grpc/proto-loader`、`mysql2`、`redis` / `ioredis`、`nodemailer`、`uuid`

```bash
cd VarifyServer
npm install
```

### 配置

编辑 `config.json`（**敏感文件，切勿提交真实值**）：

```json
{
    "email": {
      "user": "your-email@163.com",
      "pass": "your-email-auth-code"
    },
    "mysql": {
      "host": "127.0.0.1",
      "port": 3308,
      "passwd": "your_mysql_password",
      "user": "root",
      "database": "mhkh"
    },
    "redis": {
      "host": "127.0.0.1",
      "port": 6380,
      "passwd": "your_redis_password"
    }
}
```

| 字段 | 说明 |
|------|------|
| `email.user` / `email.pass` | 发件邮箱与 SMTP 授权码（非登录密码） |
| `mysql.*` | 注册信息落库用的 MySQL 连接 |
| `redis.*` | 验证码存储（`code_` 前缀） |

## 使用方法

启动服务：

```bash
npm run serve    # = node server.js，绑定 0.0.0.0:50051
```

gRPC 调用示例（grpcurl）：

```bash
grpcurl -plaintext -import-path . -proto message.proto \
  -d '{"email":"your-email@example.com"}' \
  127.0.0.1:50051 message.VarifyService/GetVarifyCode
```

或经由网关间接调用（推荐，客户端即走此链路）：

```bash
curl -X POST http://127.0.0.1:8080/get_varifycode \
  -H "Content-Type: application/json" \
  -d '{"email":"your-email@example.com"}'
```

## 目录结构

```text
VarifyServer/
├── server.js          # gRPC 服务入口（GetVarifyCode 实现）
├── email.js           # nodemailer SMTP 发送
├── mysql.js           # MySQL 访问
├── redis.js           # Redis 访问（验证码读写）
├── config.js          # 从 config.json 加载配置并导出
├── config.json        # 运行配置（占位符，勿提交真实值）
├── const.js           # 错误码与 key 前缀
├── proto.js           # message.proto 加载
├── message.proto      # gRPC 定义（公共 proto 子集）
├── package.json       # 依赖与 npm scripts
└── package-lock.json
```
