package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"strconv"
	"time"

	_ "github.com/go-sql-driver/mysql"
	"github.com/redis/go-redis/v9"
)

const monitorOverridesFile = "monitor_overrides.json"

type monitorOverrides struct {
	MySQL        string `json:"mysql"`
	Redis        string `json:"redis"`
	GateServer   string `json:"gate_server"`
	StatusServer string `json:"status_server"`
	VerifyServer string `json:"verify_server"`
	ChatServer1  string `json:"chat_server1"`
	ChatServer2  string `json:"chat_server2"`
}

func loadMonitorOverrides() monitorOverrides {
	var o monitorOverrides
	data, err := os.ReadFile(monitorOverridesFile)
	if err != nil {
		return o
	}
	_ = json.Unmarshal(data, &o)
	return o
}

func saveMonitorOverrides(o monitorOverrides) error {
	data, err := json.MarshalIndent(o, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(monitorOverridesFile, data, 0644)
}

// effectiveAddr returns override if non-empty, else env fallback, else config default.
func effectiveAddr(override, envKey, configDefault string) string {
	if override != "" {
		return override
	}
	return env(envKey, configDefault)
}

type monitorItem struct {
	Name        string `json:"name"`
	Kind        string `json:"kind"`
	Addr        string `json:"addr"`
	Online      bool   `json:"online"`
	LatencyMS   int64  `json:"latency_ms"`
	OnlineUsers int64  `json:"online_users,omitempty"`
	Error       string `json:"error,omitempty"`
}

type monitorResp struct {
	GeneratedAt string        `json:"generated_at"`
	TotalOnline int64         `json:"total_online"`
	Services    []monitorItem `json:"services"`
	Depends     []monitorItem `json:"depends"`
}

func (a *app) serviceStatus(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, 405, "方法不支持")
		return
	}

	ctx, cancel := context.WithTimeout(r.Context(), 3*time.Second)
	defer cancel()

	redisClient := a.redisClient()
	defer redisClient.Close()

	o := loadMonitorOverrides()

	depends := []monitorItem{
		a.checkMySQL(ctx, o.MySQL),
		a.checkRedis(ctx, o.Redis),
	}

	services := []monitorItem{
		checkTCPService("GateServer", "HTTP 网关", effectiveAddr(o.GateServer, "GATE_SERVER_ADDR", a.cfg.GateAddr)),
		checkTCPService("StatusServer", "gRPC 状态服务", effectiveAddr(o.StatusServer, "STATUS_SERVER_ADDR", a.cfg.StatusAddr)),
		checkTCPService("VerifyServer", "gRPC 验证码服务", effectiveAddr(o.VerifyServer, "VERIFY_SERVER_ADDR", a.cfg.VerifyAddr)),
		a.checkChatService(ctx, redisClient, "ChatServer1", effectiveAddr(o.ChatServer1, "CHAT_SERVER1_ADDR", a.cfg.ChatServer1Addr), "chatserver1"),
		a.checkChatService(ctx, redisClient, "ChatServer2", effectiveAddr(o.ChatServer2, "CHAT_SERVER2_ADDR", a.cfg.ChatServer2Addr), "chatserver2"),
	}

	var total int64
	for _, s := range services {
		total += s.OnlineUsers
	}

	writeJSON(w, monitorResp{
		GeneratedAt: time.Now().Format(time.RFC3339),
		TotalOnline: total,
		Services:    services,
		Depends:     depends,
	})
}

func (a *app) checkMySQL(ctx context.Context, overrideAddr string) monitorItem {
	start := time.Now()
	addr := overrideAddr
	if addr == "" {
		addr = env("DB_HOST", a.cfg.DBHost) + ":" + env("DB_PORT", a.cfg.DBPort)
	}
	item := monitorItem{Name: "MySQL", Kind: "数据库", Addr: addr}
	if overrideAddr != "" {
		dsn := fmt.Sprintf("%s:%s@tcp(%s)/%s?timeout=3s",
			env("DB_USER", a.cfg.DBUser), env("DB_PASSWORD", a.cfg.DBPassword),
			addr, env("DB_NAME", a.cfg.DBName))
		db, err := sql.Open("mysql", dsn)
		if err != nil {
			item.Error = err.Error()
			item.LatencyMS = time.Since(start).Milliseconds()
			return item
		}
		defer db.Close()
		if err := db.PingContext(ctx); err != nil {
			item.Error = err.Error()
			item.LatencyMS = time.Since(start).Milliseconds()
			return item
		}
	} else {
		if err := a.db.PingContext(ctx); err != nil {
			item.Error = err.Error()
			item.LatencyMS = time.Since(start).Milliseconds()
			return item
		}
	}
	item.Online = true
	item.LatencyMS = time.Since(start).Milliseconds()
	return item
}

func (a *app) checkRedis(ctx context.Context, overrideAddr string) monitorItem {
	start := time.Now()
	addr := overrideAddr
	if addr == "" {
		addr = env("REDIS_ADDR", a.cfg.RedisHost+":"+a.cfg.RedisPort)
	}
	item := monitorItem{Name: "Redis", Kind: "缓存 / 登录态", Addr: addr}
	var client *redis.Client
	if overrideAddr != "" {
		client = redis.NewClient(&redis.Options{Addr: addr, Password: env("REDIS_PASSWORD", a.cfg.RedisPassword), DB: 0})
		defer client.Close()
	} else {
		client = a.redisClient()
		defer client.Close()
	}
	if err := client.Ping(ctx).Err(); err != nil {
		item.Error = err.Error()
		item.LatencyMS = time.Since(start).Milliseconds()
		return item
	}
	item.Online = true
	item.LatencyMS = time.Since(start).Milliseconds()
	return item
}

func (a *app) checkChatService(ctx context.Context, client *redis.Client, name, addr, redisKey string) monitorItem {
	item := checkTCPService(name, "TCP 聊天服务", addr)
	count, err := client.HGet(ctx, "logincount", redisKey).Result()
	if err != nil && err != redis.Nil {
		if item.Error != "" {
			item.Error += "；Redis 在线人数读取失败：" + err.Error()
		} else {
			item.Error = "Redis 在线人数读取失败：" + err.Error()
		}
		return item
	}
	if err == redis.Nil || count == "" {
		return item
	}
	if n, parseErr := strconv.ParseInt(count, 10, 64); parseErr == nil {
		item.OnlineUsers = n
	} else {
		item.Error = "在线人数格式异常：" + count
	}
	return item
}

func checkTCPService(name, kind, addr string) monitorItem {
	start := time.Now()
	item := monitorItem{Name: name, Kind: kind, Addr: addr}
	if addr == "" {
		item.Error = "未配置地址"
		return item
	}
	conn, err := net.DialTimeout("tcp", addr, 1200*time.Millisecond)
	item.LatencyMS = time.Since(start).Milliseconds()
	if err != nil {
		item.Error = err.Error()
		return item
	}
	_ = conn.Close()
	item.Online = true
	return item
}

// monitorConfigResp is the response for GET /api/monitor/config.
type monitorConfigResp struct {
	Overrides monitorOverrides `json:"overrides"`
	Defaults  monitorOverrides `json:"defaults"`
}

func (a *app) getMonitorConfig(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, 405, "方法不支持")
		return
	}
	writeJSON(w, monitorConfigResp{
		Overrides: loadMonitorOverrides(),
		Defaults: monitorOverrides{
			MySQL:        env("DB_HOST", a.cfg.DBHost) + ":" + env("DB_PORT", a.cfg.DBPort),
			Redis:        env("REDIS_ADDR", a.cfg.RedisHost+":"+a.cfg.RedisPort),
			GateServer:   a.cfg.GateAddr,
			StatusServer: a.cfg.StatusAddr,
			VerifyServer: a.cfg.VerifyAddr,
			ChatServer1:  a.cfg.ChatServer1Addr,
			ChatServer2:  a.cfg.ChatServer2Addr,
		},
	})
}

func (a *app) updateMonitorConfig(w http.ResponseWriter, r *http.Request, operator string) {
	if r.Method != http.MethodPost {
		writeErr(w, 405, "方法不支持")
		return
	}
	var o monitorOverrides
	if !decodeJSON(w, r, &o) {
		return
	}
	if err := saveMonitorOverrides(o); err != nil {
		writeErr(w, 500, "保存失败："+err.Error())
		return
	}
	a.logOperation(r, operator, "系统监控", "修改监控地址", "", "", nil,
		"修改服务监控地址",
		"MySQL="+o.MySQL+" Redis="+o.Redis+" GateServer="+o.GateServer+" StatusServer="+o.StatusServer+" VerifyServer="+o.VerifyServer+" ChatServer1="+o.ChatServer1+" ChatServer2="+o.ChatServer2)
	writeJSON(w, map[string]string{"status": "ok"})
}
