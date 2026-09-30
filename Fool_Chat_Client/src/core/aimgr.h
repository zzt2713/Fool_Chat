#ifndef AIMGR_H
#define AIMGR_H

#include "F_singleton.h"
#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>

// AI 对话消息（role: system / user / assistant / tool）
struct AiMessage {
    QString role;       // 角色
    QString content;    // 文本内容（tool 时为工具执行结果 JSON 串）
    QString toolCallId; // role=tool 时对应 tool_calls[].id
    QJsonArray toolCalls; // role=assistant 且发起工具调用时，原样保存响应中的 tool_calls
};

/**
 * @brief AI 机器人管理器：直连 OpenAI 兼容接口（HTTP 异步），支持 function calling
 *
 * - 单发锁：同一时刻只处理一条请求（_busy 为唯一真源）
 * - 工具两轮循环：tool_calls → 执行 → 回填 tool 消息 → 再请求，上限 3 次请求
 * - 降级：接口不认 tools 参数（HTTP 400）时改走 system 指令 + 单行 JSON 协议
 * - 对话历史由调用方每次传入（内存态），本类不持久化
 */
class AiMgr : public QObject, public F_Singleton<AiMgr>
{
    Q_OBJECT
public:
    // main 启动时从 config.ini [ai] 段注入；允许空（未配置时 SendChat 直接报错）
    void Init(const QString& baseUrl, const QString& model, const QString& apiKey);
    // history 为该会话既有历史（不含本轮 userText）
    void SendChat(const QVector<AiMessage>& history, const QString& userText);
    bool IsBusy() const { return _busy; }

signals:
    void sig_ai_chunk(const QString& delta);  // 流式增量片段（闲聊路径）
    void sig_ai_reply(const QString& text);   // 最终自然语言回复（工具调用完成后）
    void sig_ai_error(const QString& reason); // 失败原因（不含任何密钥信息）
    void sig_window_op(const QString& op, const QString& arg); // open_page / window_minimize / window_center

private:
    friend class F_Singleton<AiMgr>;
    AiMgr();

    QJsonArray serializeMessages() const;
    QJsonArray buildTools() const;
    QString systemPrompt() const;

    void postCompletion(bool withTools);
    void onResponse(QNetworkReply* reply, bool withTools);
    // 流式 SSE 增量处理（闲聊路径）
    void handleStreamReply(QNetworkReply* reply);
    // 消息像"操作指令"才带 tools（闲聊1发结束，避免连发触发429限流）
    static bool looksLikeCommand(const QString& text);
    // 执行单个 tool_call，返回 {"status":"ok"} 或 {"status":"error",...}
    QString executeToolCall(const QJsonObject& toolCall);
    // 降级模式：从文本里抠出 {"tool":...} 单行 JSON，返回空表示不是工具指令
    QJsonObject extractToolJson(const QString& text) const;

    void finishReply(const QString& text);
    void finishError(const QString& reason);

    QNetworkAccessManager _nam;
    QString _baseUrl;
    QString _model;
    QString _apiKey;

    QVector<AiMessage> _messages; // 本轮请求累计（system+history+user+工具往返）
    bool _busy = false;           // 单发锁
    bool _toolsUnsupported = false; // 进程内记忆：接口不认 tools
    bool _degradeExecuted = false;  // 降级模式已执行过一次工具
    int _toolRounds = 0;             // 已因 tool_calls 发起的续请求轮数
    int _retryCount = 0;             // 429 限流退避重试计数
    QByteArray _sseBuf;              // 流式响应拼包缓冲
    QString _streamText;             // 流式累计全文
    bool _streamDone = false;        // 收到 [DONE]
};

#endif // AIMGR_H
