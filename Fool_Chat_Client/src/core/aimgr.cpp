#include "aimgr.h"
#include "ElaTheme.h"
#include "usermgr.h"
#include "tcpmgr.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>

namespace {
// 降级模式下追加到 system 提示末尾的单行 JSON 协议说明
const char* kDegradeHint =
    " 如需操作软件，只输出一行 JSON，格式："
    "{\"tool\":\"switch_theme\",\"args\":{\"mode\":\"dark\"}}；"
    "可用工具：switch_theme(mode: dark|light)、open_page(page: chat/contacts/notice/"
    "dynamics/music/editor/settings/about/admin/setting)、window_minimize()、window_center()、"
    "set_font_size(size: small|standard|large)、edit_remark(friend, remark)、"
    "clear_cache(type: wallpaper|music)、"
    "set_notify(on?: boolean 查询或开关消息通知)、query_contacts()、query_notices()、"
    "publish_dynamic(content: 动态文本)。"
    "JSON 必须严格是 {\"tool\":\"工具名\",\"args\":{...}} 这一种格式，"
    "输出其它任何 JSON（如 {\"theme\":...}）都不会被执行；不需要操作时直接输出普通回复，"
    "回复正文里不要出现 JSON。";
} // namespace

AiMgr::AiMgr() = default;

void AiMgr::Init(const QString& baseUrl, const QString& model, const QString& apiKey)
{
    _baseUrl = baseUrl.trimmed();
    while (_baseUrl.endsWith('/')) {
        _baseUrl.chop(1);
    }
    _model = model.trimmed();
    _apiKey = apiKey.trimmed();
}

QString AiMgr::systemPrompt() const
{
    return QStringLiteral(
        "你是 FoolChat 即时通讯软件的内置 AI 助手。回复要求："
        "1. 简洁友好的中文，先给结论再展开；"
        "2. 超过三点用无序列表，步骤用有序列表；"
        "3. 关键词用 **加粗**，代码用带语言标记的 ```代码块，不要用标题和表格；"
        "4. 操作软件（主题、页面、窗口、字体、备注、缓存）或查询/修改应用数据"
        "（通讯录好友在线状态、通知公告、消息通知开关、发布动态）时优先调用提供的工具，"
        "不要编造工具执行结果；"
        "5. 只如实汇报工具真实返回的结果，禁止虚构未提供的功能或效果"
        "（例如磨砂、毛玻璃、dark_frosted 等不存在的主题）；主题只有 dark 和 light 两种；"
        "回复正文里永远不要输出 JSON 或协议格式内容。");
}

QJsonArray AiMgr::buildTools() const
{
    auto fn = [](const char* name, const char* desc, const QJsonObject& props, const QJsonArray& required) {
        QJsonObject function;
        function["name"] = QString::fromUtf8(name);
        function["description"] = QString::fromUtf8(desc);
        QJsonObject schema;
        schema["type"] = "object";
        schema["properties"] = props;
        if (!required.isEmpty()) {
            schema["required"] = required;
        }
        function["parameters"] = schema;
        QJsonObject tool;
        tool["type"] = "function";
        tool["function"] = function;
        return tool;
    };

    QJsonObject modeProp;
    modeProp["type"] = "string";
    modeProp["enum"] = QJsonArray{"dark", "light"};

    QJsonObject pageProp;
    pageProp["type"] = "string";
    pageProp["enum"] = QJsonArray{"chat", "contacts", "notice", "dynamics", "music",
                                  "editor", "settings", "about", "admin", "setting"};

    QJsonObject sizeProp;
    sizeProp["type"] = "string";
    sizeProp["enum"] = QJsonArray{"small", "standard", "large"};

    QJsonObject friendProp;
    friendProp["type"] = "string";
    QJsonObject remarkProp;
    remarkProp["type"] = "string";

    QJsonObject cacheProp;
    cacheProp["type"] = "string";
    cacheProp["enum"] = QJsonArray{"wallpaper", "music"};

    QJsonObject onProp;
    onProp["type"] = "boolean";

    QJsonObject contentProp;
    contentProp["type"] = "string";

    QJsonArray tools;
    tools.append(fn("switch_theme", "切换软件主题（暗黑/亮色）", {{"mode", modeProp}}, {"mode"}));
    tools.append(fn("open_page", "打开软件内的指定页面", {{"page", pageProp}}, {"page"}));
    tools.append(fn("window_minimize", "最小化主窗口", {}, {}));
    tools.append(fn("window_center", "把主窗口移到屏幕中央", {}, {}));
    tools.append(fn("set_font_size", "设置全局字体大小", {{"size", sizeProp}}, {"size"}));
    tools.append(fn("edit_remark", "修改好友备注（仅自己可见）", {{"friend", friendProp}, {"remark", remarkProp}}, {"friend", "remark"}));
    tools.append(fn("clear_cache", "清理缓存（壁纸或音乐库）", {{"type", cacheProp}}, {"type"}));
    tools.append(fn("set_notify", "查询或开关桌面消息通知（不带 on 参数为查询当前状态）", {{"on", onProp}}, {}));
    tools.append(fn("query_contacts", "查询通讯录好友及在线状态", {}, {}));
    tools.append(fn("query_notices", "查询通知公告列表", {}, {}));
    tools.append(fn("publish_dynamic", "发布一条文字动态到动态页", {{"content", contentProp}}, {"content"}));
    return tools;
}

QJsonArray AiMgr::serializeMessages() const
{
    QJsonArray arr;
    for (const auto& m : _messages) {
        QJsonObject o;
        if (m.role == QLatin1String("assistant") && !m.toolCalls.isEmpty()) {
            o["role"] = "assistant";
            o["tool_calls"] = m.toolCalls;
            if (!m.content.isEmpty()) {
                o["content"] = m.content;
            }
        } else if (m.role == QLatin1String("tool")) {
            o["role"] = "tool";
            o["tool_call_id"] = m.toolCallId;
            o["content"] = m.content;
        } else {
            o["role"] = m.role;
            o["content"] = m.content;
        }
        arr.append(o);
    }
    return arr;
}

void AiMgr::SendChat(const QVector<AiMessage>& history, const QString& userText)
{
    if (_baseUrl.isEmpty() || _apiKey.isEmpty() || _model.isEmpty()) {
        emit sig_ai_error(QStringLiteral("AI 未配置：config.ini 缺少 [ai] 段"));
        return;
    }
    if (_busy) {
        emit sig_ai_error(QStringLiteral("上一条回复还没完成，请稍候"));
        return;
    }
    _busy = true;
    _toolRounds = 0;
    _degradeExecuted = false;
    _retryCount = 0;
    _sseBuf.clear();
    _streamText.clear();
    _streamDone = false;

    _messages.clear();
    AiMessage sys;
    sys.role = "system";
    sys.content = systemPrompt();
    if (_toolsUnsupported) {
        sys.content += QString::fromUtf8(kDegradeHint);
    }
    _messages.append(sys);
    _messages += history;
    AiMessage user;
    user.role = "user";
    user.content = userText;
    _messages.append(user);

    // 闲聊不带 tools（单发结束）；只有像操作指令的消息才带，压住请求频率防429
    postCompletion(!_toolsUnsupported && looksLikeCommand(userText));
}

bool AiMgr::looksLikeCommand(const QString& text)
{
    static const QStringList keys = {
        QStringLiteral("主题"), QStringLiteral("暗黑"), QStringLiteral("深色"),
        QStringLiteral("浅色"), QStringLiteral("亮色"), QStringLiteral("切换"),
        QStringLiteral("打开"), QStringLiteral("跳转"), QStringLiteral("最小化"),
        QStringLiteral("居中"), QStringLiteral("窗口"), QStringLiteral("页面"),
        QStringLiteral("设置"), QStringLiteral("动态"), QStringLiteral("音乐"),
        QStringLiteral("通讯录"), QStringLiteral("通知"), QStringLiteral("编辑器"),
        QStringLiteral("在线"), QStringLiteral("发布"), QStringLiteral("开关"),
        QStringLiteral("备注"), QStringLiteral("字体"), QStringLiteral("字号"),
        QStringLiteral("缓存"), QStringLiteral("清理"), QStringLiteral("好友"),
        QStringLiteral("dark"), QStringLiteral("light"), QStringLiteral("theme")
    };
    const QString lower = text.toLower();
    for (const QString& key : keys) {
        if (lower.contains(key)) {
            return true;
        }
    }
    return false;
}

void AiMgr::postCompletion(bool withTools)
{
    QJsonObject body;
    body["model"] = _model;
    body["messages"] = serializeMessages();
    if (withTools) {
        body["tools"] = buildTools();
        body["tool_choice"] = "auto";
    }
    // 闲聊流式；工具调用与降级 JSON 协议保持非流式（整包解析）
    const bool streaming = !withTools && !_toolsUnsupported;
    body["stream"] = streaming;

    QNetworkRequest req(QUrl(_baseUrl + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", "Bearer " + _apiKey.toUtf8());
    req.setTransferTimeout(60000);

    QNetworkReply* reply = _nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (!streaming) {
        connect(reply, &QNetworkReply::finished, this, [this, reply, withTools]() {
            onResponse(reply, withTools); // 内部先读数据再 deleteLater
        });
        return;
    }
    handleStreamReply(reply);
}

void AiMgr::handleStreamReply(QNetworkReply* reply)
{
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        _sseBuf += reply->readAll();
        forever {
            int nl = _sseBuf.indexOf('\n');
            if (nl < 0) {
                break;
            }
            QByteArray line = _sseBuf.left(nl).trimmed();
            _sseBuf.remove(0, nl + 1);
            if (!line.startsWith("data:")) {
                continue;
            }
            QByteArray payload = line.mid(5).trimmed();
            if (payload == "[DONE]") {
                _streamDone = true;
                continue;
            }
            QJsonObject chunk = QJsonDocument::fromJson(payload).object();
            QJsonArray choices = chunk.value(QLatin1String("choices")).toArray();
            if (choices.isEmpty()) {
                continue;
            }
            QString delta = choices.at(0).toObject()
                                .value(QLatin1String("delta")).toObject()
                                .value(QLatin1String("content")).toString();
            if (!delta.isEmpty()) {
                _streamText += delta;
                emit sig_ai_chunk(delta);
            }
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool netErr = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (netErr) {
            if (status == 429 && _retryCount < 2) {
                ++_retryCount;
                QTimer::singleShot(1500 * _retryCount, this, [this]() {
                    postCompletion(false);
                });
                return;
            }
            finishError(QStringLiteral("AI 请求失败（%1）").arg(status > 0 ? status : 0));
            return;
        }
        finishReply(_streamText);
    });
}

void AiMgr::onResponse(QNetworkReply* reply, bool withTools)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const bool netErr = reply->error() != QNetworkReply::NoError;
    reply->deleteLater();

    if (netErr) {
        // 接口不认 tools 参数 → 降级为 system 指令 + 单行 JSON 协议
        if (withTools && status == 400 &&
            (body.contains("tools") || body.contains("unknown") || body.contains("function"))) {
            _toolsUnsupported = true;
            if (!_messages.isEmpty() && _messages[0].role == QLatin1String("system")) {
                _messages[0].content += QString::fromUtf8(kDegradeHint);
            }
            postCompletion(false);
            return;
        }
        // 限流429：指数退避后用同一份消息重发（最多2次）
        if (status == 429 && _retryCount < 2) {
            ++_retryCount;
            QTimer::singleShot(1500 * _retryCount, this, [this, withTools]() {
                postCompletion(withTools);
            });
            return;
        }
        finishError(QStringLiteral("AI 请求失败（%1）").arg(status > 0 ? status : int(reply->error())));
        return;
    }

    QJsonObject root = QJsonDocument::fromJson(body).object();
    QJsonArray choices = root.value(QLatin1String("choices")).toArray();
    if (choices.isEmpty()) {
        finishError(QStringLiteral("AI 返回为空"));
        return;
    }
    QJsonObject msg = choices.at(0).toObject().value(QLatin1String("message")).toObject();
    QJsonArray toolCalls = msg.value(QLatin1String("tool_calls")).toArray();

    if (!toolCalls.isEmpty()) {
        if (_toolRounds >= 2) {
            // 两轮工具后强制收尾，避免失控循环
            finishReply(msg.value(QLatin1String("content")).toString());
            return;
        }
        ++_toolRounds;

        AiMessage assistant;
        assistant.role = "assistant";
        assistant.content = msg.value(QLatin1String("content")).toString();
        assistant.toolCalls = toolCalls;
        _messages.append(assistant);

        for (const auto& tc : toolCalls) {
            AiMessage toolMsg;
            toolMsg.role = "tool";
            toolMsg.toolCallId = tc.toObject().value(QLatin1String("id")).toString();
            toolMsg.content = executeToolCall(tc.toObject());
            _messages.append(toolMsg);
        }
        // 工具执行完强制无 tools 收尾：工具路径总请求封顶2发（防连发429）
        postCompletion(false);
        return;
    }

    QString text = msg.value(QLatin1String("content")).toString();

    // 降级模式：模型按 system 指令回了单行 JSON 工具调用
    if (!_toolsUnsupported) {
        finishReply(text);
        return;
    }
    QJsonObject toolJson = extractToolJson(text);
    if (!toolJson.isEmpty() && toolJson.contains(QLatin1String("tool"))) {
        if (_degradeExecuted) {
            // 已执行过一次工具仍回 JSON → 当普通文本放行，防止死循环
            finishReply(text);
            return;
        }
        _degradeExecuted = true;
        QJsonObject fakeCall;
        fakeCall["id"] = QStringLiteral("degrade_1");
        fakeCall["type"] = "function";
        QJsonObject function;
        function["name"] = toolJson.value(QLatin1String("tool")).toString();
        function["arguments"] = QString::fromUtf8(
            QJsonDocument(toolJson.value(QLatin1String("args")).toObject())
                .toJson(QJsonDocument::Compact));
        fakeCall["function"] = function;

        AiMessage assistant;
        assistant.role = "assistant";
        assistant.content = text;
        assistant.toolCalls = QJsonArray{fakeCall};
        _messages.append(assistant);

        AiMessage toolMsg;
        toolMsg.role = "tool";
        toolMsg.toolCallId = fakeCall["id"].toString();
        toolMsg.content = executeToolCall(fakeCall);
        _messages.append(toolMsg);

        AiMessage hint;
        hint.role = "user";
        hint.content = QStringLiteral("工具执行完成，请基于结果给出简短的自然语言回复。");
        _messages.append(hint);
        postCompletion(false);
        return;
    }

    if (text.trimmed().isEmpty()) {
        finishError(QStringLiteral("AI 返回内容为空"));
        return;
    }
    finishReply(text);
}

QString AiMgr::executeToolCall(const QJsonObject& toolCall)
{
    const QJsonObject function = toolCall.value(QLatin1String("function")).toObject();
    const QString name = function.value(QLatin1String("name")).toString();
    const QJsonObject args = QJsonDocument::fromJson(
        function.value(QLatin1String("arguments")).toString().toUtf8()).object();

    if (name == QLatin1String("switch_theme")) {
        const QString mode = args.value(QLatin1String("mode")).toString();
        if (mode != QLatin1String("dark") && mode != QLatin1String("light")) {
            return QStringLiteral("{\"status\":\"error\",\"reason\":\"mode must be dark or light\"}");
        }
        eTheme->setThemeMode(mode == QLatin1String("dark") ? ElaThemeType::Dark : ElaThemeType::Light);
        return QStringLiteral("{\"status\":\"ok\"}");
    }
    if (name == QLatin1String("open_page") ||
        name == QLatin1String("window_minimize") ||
        name == QLatin1String("window_center")) {
        // 消费方（C_Window）以 DirectConnection 同线程执行，返回即视为成功
        emit sig_window_op(name, args.value(QLatin1String("page")).toString());
        return QStringLiteral("{\"status\":\"ok\"}");
    }
    if (name == QLatin1String("set_font_size")) {
        const QString size = args.value(QLatin1String("size")).toString();
        if (size != QLatin1String("small") && size != QLatin1String("standard") && size != QLatin1String("large")) {
            return QStringLiteral("{\"status\":\"error\",\"reason\":\"size must be small|standard|large\"}");
        }
        emit sig_window_op("set_font_size", size);
        return QStringLiteral("{\"status\":\"ok\"}");
    }
    if (name == QLatin1String("edit_remark")) {
        // 参数拼成 "好友名|备注" 交给窗口层解析执行
        emit sig_window_op("edit_remark",
                           args.value(QLatin1String("friend")).toString() + "|" +
                               args.value(QLatin1String("remark")).toString());
        return QStringLiteral("{\"status\":\"ok\"}");
    }
    if (name == QLatin1String("clear_cache")) {
        const QString type = args.value(QLatin1String("type")).toString();
        if (type != QLatin1String("wallpaper") && type != QLatin1String("music")) {
            return QStringLiteral("{\"status\":\"error\",\"reason\":\"type must be wallpaper|music\"}");
        }
        emit sig_window_op("clear_cache", type);
        return QStringLiteral("{\"status\":\"ok\"}");
    }
    if (name == QLatin1String("set_notify")) {
        // 通知开关持久化在 ui_settings.ini，通知弹窗每次触发时实时读取 → 保存即生效
        QSettings settings(QCoreApplication::applicationDirPath() + "/ui_settings.ini",
                           QSettings::IniFormat);
        const bool on = args.contains(QLatin1String("on"))
                            ? args.value(QLatin1String("on")).toBool()
                            : settings.value("notifyOn", true).toBool();
        if (args.contains(QLatin1String("on"))) {
            settings.setValue("notifyOn", on);
            settings.sync();
        }
        return QStringLiteral("{\"status\":\"ok\",\"data\":{\"notifyOn\":%1}}")
            .arg(on ? "true" : "false");
    }
    if (name == QLatin1String("query_contacts")) {
        QJsonArray friends;
        int online = 0;
        for (const auto& f : UserMgr::GetInstance()->GetAllFriends()) {
            if (f == nullptr) {
                continue;
            }
            QJsonObject item;
            item["name"] = f->DisplayName();
            item["online"] = (f->_status == 1);
            friends.append(item);
            if (f->_status == 1) {
                ++online;
            }
        }
        QJsonObject data;
        data["total"] = static_cast<int>(friends.size());
        data["online"] = online;
        data["friends"] = friends;
        return QString::fromUtf8(
            QJsonDocument(QJsonObject{{"status", "ok"}, {"data", data}})
                .toJson(QJsonDocument::Compact));
    }
    if (name == QLatin1String("query_notices")) {
        const QJsonArray notices = UserMgr::GetInstance()->GetNoticeCache();
        QJsonObject data;
        data["notices"] = notices;
        if (notices.isEmpty()) {
            data["note"] = QStringLiteral("通知列表为空或尚未加载");
        }
        return QString::fromUtf8(
            QJsonDocument(QJsonObject{{"status", "ok"}, {"data", data}})
                .toJson(QJsonDocument::Compact));
    }
    if (name == QLatin1String("publish_dynamic")) {
        const QString content = args.value(QLatin1String("content")).toString().trimmed();
        if (content.isEmpty()) {
            return QStringLiteral("{\"status\":\"error\",\"reason\":\"content is empty\"}");
        }
        QJsonObject obj;
        obj["uid"] = UserMgr::GetInstance()->GetUid();
        obj["content"] = content;
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_PUBLISH_DYNAMIC_REQ,
                                                  QJsonDocument(obj).toJson(QJsonDocument::Compact));
        return QStringLiteral("{\"status\":\"ok\",\"note\":\"动态已提交发布\"}");
    }
    return QStringLiteral("{\"status\":\"error\",\"reason\":\"unknown tool\"}");
}

QJsonObject AiMgr::extractToolJson(const QString& text) const
{
    const int s = text.indexOf(QLatin1Char('{'));
    const int e = text.lastIndexOf(QLatin1Char('}'));
    if (s < 0 || e <= s) {
        return {};
    }
    const QJsonObject obj = QJsonDocument::fromJson(text.mid(s, e - s + 1).toUtf8()).object();
    return obj.contains(QLatin1String("tool")) ? obj : QJsonObject();
}

void AiMgr::finishReply(const QString& text)
{
    _busy = false;
    _retryCount = 0;
    emit sig_ai_reply(text);
}

void AiMgr::finishError(const QString& reason)
{
    _busy = false;
    _retryCount = 0;
    // 注意：reason 内不得拼入 apiKey / 请求头
    emit sig_ai_error(reason);
}
