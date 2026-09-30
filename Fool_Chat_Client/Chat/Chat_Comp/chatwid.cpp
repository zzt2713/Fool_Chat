#include "chatwid.h"
#include "ui_chatwid.h"
#include <QStyleOption>
#include <QPainter>
#include "ranimg.h"
#include "global.h"
#include "ElaImageCard.h"
#include "textbubble.h"
#include "typingdots.h"
#include "themedtext.h"
#include "picturebubble.h"
#include "usermgr.h"
#include "chatitembase.h"
#include <QJsonDocument>
#include <QTimer>
#include <QDateTime>
#include <QMenu>
#include <QSplitter>
#include <QSplitterHandle>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QLabel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "ElaText.h"
#include "ElaDialog.h"
#include "ElaPushButton.h"
#include "tcpmgr.h"
#include <QUuid>
#include "aimgr.h"
#include "msgtip.h"
#include "callui.h"
#include <QStringLiteral>

ChatWid::ChatWid(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWid),_user_info(nullptr)
{
    ui->setupUi(this);
    // ElaText 颜色绑定主题（自带样式表不吃 palette 更新）
    ui->title_lb->setTextStyle(ElaTextType::Body);
    BindTextToTheme(ui->title_lb);
    // 输入框文字颜色同样绑定主题
    auto applyInputColor = [edit = ui->chatEdit](ElaThemeType::ThemeMode mode) {
        const QColor color = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
        edit->setStyleSheet(QStringLiteral("QTextEdit{color:%1;}").arg(color.name()));
    };
    applyInputColor(eTheme->getThemeMode());
    connect(eTheme, &ElaTheme::themeModeChanged, ui->chatEdit, applyInputColor);
    // 聊天区任意位置可拖入图片文件
    setAcceptDrops(true);

    ui->emo_btn->setIsTransparent(true);
    ui->emo_btn->setElaIcon(ElaIconType::FaceSmile);
    ui->emo_btn->setToolTip("表情");

    ui->file_btn->setIsTransparent(true);
    ui->file_btn->setElaIcon(ElaIconType::File);
    ui->file_btn->setToolTip("文件");

    ui->video_btn->setIsTransparent(true);
    ui->video_btn->setElaIcon(ElaIconType::Video);
    ui->video_btn->setToolTip("视频通话");
    connect(ui->video_btn, &ElaToolButton::clicked, this, [this]() {
        const int peer = (_user_info != nullptr) ? _user_info->_uid : -1;
        TryStartCall(this, peer, CallType::VIDEO);
    });

    ui->phone_btn->setIsTransparent(true);
    ui->phone_btn->setElaIcon(ElaIconType::Phone);
    ui->phone_btn->setToolTip("语音通话");
    connect(ui->phone_btn, &ElaToolButton::clicked, this, [this]() {
        const int peer = (_user_info != nullptr) ? _user_info->_uid : -1;
        TryStartCall(this, peer, CallType::VOICE);
    });

    ui->more_btn->setIsTransparent(true);
    ui->more_btn->setElaIcon(ElaIconType::Ellipsis);
    ui->more_btn->setToolTip("更多");
    connect(ui->more_btn, &ElaToolButton::clicked, this, [this]() {
        QMenu menu(this);
        QAction* summaryAct = menu.addAction(QStringLiteral("AI 总结本会话"));
        connect(summaryAct, &QAction::triggered, this, [this]() {
            if (_user_info == nullptr || AiMgr::GetInstance()->IsBusy()) {
                return;
            }
            QString convo;
            for (const auto& m : _user_info->_chat_msgs) {
                convo += (m->_from_uid == UserMgr::GetInstance()->GetUid() ? QStringLiteral("我：")
                                                                         : _user_info->DisplayName() + QStringLiteral("："));
                convo += m->_msg_content + QStringLiteral("\n");
            }
            if (convo.isEmpty()) {
                ADDMSG(ElaMessageBarType::Top, "当前会话还没有消息", this, 2, 2000);
                return;
            }
            // 记录目标会话：回复到达时落回该会话，而不是 AI 会话
            _aiSummaryTargetUid = _user_info->_uid;
            appendAiPlaceholder();
            AiMgr::GetInstance()->SendChat({},
                                           QStringLiteral("我是 %1，正在和 %2 聊天。请用要点简洁总结以下聊天记录，开头点明这是与 %2 的对话：\n")
                                               .arg(UserMgr::GetInstance()->GetUserInfo()->DisplayName(),
                                                    _user_info->DisplayName())
                                           + convo);
        });
        menu.exec(QCursor::pos());
    });

    ui->status_lb->setStatus(true);

    ui->avatar_wid->setFixedSize(35,35);
    ui->avatar_wid->setBorderRadius(35 / 2);
    ui->avatar_wid->setCardImage(IMG_IMAGE("pp"));
    ui->avatar_wid->setIsPreserveAspectCrop(true);

    connect(ui->chatEdit, &MessageTextEdit::send, this, &ChatWid::on_send_btn_clicked);

    // QQ 式：聊天区/输入区间隔条默认隐形，悬停显示细线，拖拽调节输入框高度
    QVBoxLayout* chatLayout = qobject_cast<QVBoxLayout*>(ui->chat_dat_wid->layout());
    if (chatLayout != nullptr) {
        auto* bottomWidget = new QWidget(ui->chat_dat_wid);
        auto* bottomLayout = new QVBoxLayout(bottomWidget);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(0);
        for (QWidget* w : QList<QWidget*>{ui->tool_wid, ui->chatEdit, ui->send_wid}) {
            chatLayout->removeWidget(w);
            bottomLayout->addWidget(w);
        }
        chatLayout->removeWidget(ui->chat_data_list);
        auto* splitter = new QSplitter(Qt::Vertical, ui->chat_dat_wid);
        splitter->setHandleWidth(6);
        splitter->setChildrenCollapsible(false);
        splitter->setStyleSheet(QStringLiteral(
            "QSplitter::handle { background: transparent; }"
            "QSplitter::handle:hover { background: rgba(128, 128, 128, 0.45); }"));
        if (QSplitterHandle* handle = splitter->handle(0)) {
            handle->setCursor(Qt::SplitVCursor);
        }
        splitter->addWidget(ui->chat_data_list);
        splitter->addWidget(bottomWidget);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 0);
        splitter->setSizes({400, 160});
        chatLayout->addWidget(splitter);
    }

    // Ctrl+滚轮缩放气泡字体
    connect(ui->chat_data_list, &ChatView::sigFontZoom, this, [this](bool zoomIn) {
        _chatFontScale = qBound(0.6, _chatFontScale * (zoomIn ? 1.1 : 1.0 / 1.1), 2.0);
        applyChatFontScale();
    });

    // 消息送达回执：按 msgid 刷新气泡状态
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_msg_delivered, this, [this](QStringList msgIds) {
        for (const QString& id : msgIds) {
            auto it = _pendingMsgs.find(id);
            if (it != _pendingMsgs.end()) {
                if (it.value() != nullptr) {
                    it.value()->setSendStatus(1);
                }
                _pendingMsgs.erase(it);
            }
        }
    });

    // 断线时未收到回执的统一标为发送失败
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_link_state, this, [this](bool online) {
        if (online) {
            return;
        }
        for (auto it = _pendingMsgs.begin(); it != _pendingMsgs.end(); ++it) {
            if (it.value() != nullptr) {
                it.value()->setSendStatus(2);
            }
        }
        _pendingMsgs.clear();
    });

    // AI 回复/失败回调（单实例 chat_page，不会重复连接）
    connect(AiMgr::GetInstance().get(), &AiMgr::sig_ai_reply, this, &ChatWid::slot_ai_reply);
    connect(AiMgr::GetInstance().get(), &AiMgr::sig_ai_error, this, &ChatWid::slot_ai_error);

    // 流式渲染节流：整篇 Markdown 每个 token 重排一次会拖死 UI 线程（切会话卡顿源）
    _aiStreamFlushTimer = new QTimer(this);
    _aiStreamFlushTimer->setSingleShot(true);
    _aiStreamFlushTimer->setInterval(50);
    connect(_aiStreamFlushTimer, &QTimer::timeout, this, [this]() {
        if (_aiTypingBubble.isNull()) {
            return;
        }
        _aiTypingBubble->setMarkdownText(_aiStreamText);
    });

    // 流式增量：首个增量把点点占位换成文本气泡，之后按节流批量渲染
    connect(AiMgr::GetInstance().get(), &AiMgr::sig_ai_chunk, this, [this](const QString& delta) {
        if (_aiPending.isNull()) {
            return;
        }
        const bool firstChunk = _aiTypingBubble.isNull();
        if (firstChunk) {
            auto* bubble = new TextBubble(ChatRole::Other, QString());
            _aiPending->setWidget(bubble);
            _aiPending->setFontScale(_chatFontScale);
            _aiTypingBubble = bubble;
        }
        _aiStreamText += delta;
        if (firstChunk) {
            _aiTypingBubble->setMarkdownText(_aiStreamText);
        }
        else if (!_aiStreamFlushTimer->isActive()) {
            _aiStreamFlushTimer->start();
        }
    });
}

ChatWid::~ChatWid()
{
    delete ui;
}

void ChatWid::SetStatus(bool b_status)
{
    ui->status_lb->setStatus(b_status);
}

void ChatWid::SetUserInfo(std::shared_ptr<UserInfo> user_info)
{
    if(!user_info){
        return;
    }

    _user_info = user_info;
    _lastMsgTs = QDateTime();
    ui->title_lb->setText(_user_info->DisplayName());
    // 会话头像跟随对方资料，icon 为空或加载失败时回退随机头像
    QPixmap iconPixmap(_user_info->_icon);
    ui->avatar_wid->setCardImage(iconPixmap.isNull() ? IMG_IMAGE("pp") : iconPixmap.toImage());
    ui->avatar_wid->update();
    // 头像旁状态点：AI 助手恒为在线，其余按 friend_list 的 status 快照
    SetStatus(_user_info->_uid == -1 || _user_info->_status == 1);
    ui->chat_data_list->removeAllItem();

    for(auto & msg : user_info->_chat_msgs){
        AppendChatMsg(msg, false);
    }
}

void ChatWid::AppendChatMsg(std::shared_ptr<TextChatData> msg, bool live)
{
    if (live) {
        appendTimeDividerIfNeeded();
    }
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;
    //todo... 添加聊天显示
    if (msg->_from_uid == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);

        pChatItem->setUserIcon(QPixmap(self_info->_icon));
        pChatItem->setFontScale(_chatFontScale);
        connect(pChatItem, &ChatItemBase::sigContextMenuRequested, this, &ChatWid::showItemContextMenu);
        TextBubble* pBubble = new TextBubble(role, msg->_msg_content);
        pChatItem->setWidget(pBubble);
        ui->chat_data_list->appendItem(pChatItem);
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->_from_uid);
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->DisplayName());
        pChatItem->setUserIcon(QPixmap(friend_info->_icon));
        pChatItem->setUserId(friend_info->_uid);
        pChatItem->setFontScale(_chatFontScale);
        connect(pChatItem, &ChatItemBase::sigIconClicked, this, &ChatWid::sig_show_profile);
        connect(pChatItem, &ChatItemBase::sigContextMenuRequested, this, &ChatWid::showItemContextMenu);
        TextBubble* pBubble = new TextBubble(role, msg->_msg_content);
        // AI 回复按 Markdown 渲染，普通好友消息保持纯文本
        if (msg->_from_uid == -1) {
            pBubble->setMarkdownText(msg->_msg_content);
        }
        pChatItem->setWidget(pBubble);
        ui->chat_data_list->appendItem(pChatItem);
    }

}

// 插入 AI 占位气泡（三个弹跳圆点），首个流式增量到达后换成文本气泡
void ChatWid::appendAiPlaceholder()
{
    auto aiInfo = UserMgr::GetInstance()->GetFriendById(-1);
    ChatItemBase* item = new ChatItemBase(ChatRole::Other);
    item->setUserName(aiInfo ? aiInfo->_name : QStringLiteral("AI助手"));
    item->setUserIcon(QPixmap(aiInfo ? aiInfo->_icon : QStringLiteral(":/icons/image.png")));
    auto* bubble = new TypingDotsBubble(ChatRole::Other);
    item->setWidget(bubble);
    item->setFontScale(_chatFontScale);
    ui->chat_data_list->appendItem(item);
    _aiPending = item;
    _aiTypingBubble.clear();
    _aiStreamText.clear();
}

// 移除占位气泡；切会话后 removeAllItem 已把 widget 删掉时 QPointer 为空，安全跳过
void ChatWid::removeAiPlaceholder()
{
    _aiTypingBubble.clear();
    if (_aiPending.isNull()) {
        return;
    }
    ui->chat_data_list->removeItem(_aiPending.data());
    _aiPending.clear();
}

void ChatWid::applyChatFontScale()
{
    for (QWidget* w : ui->chat_data_list->items()) {
        if (auto* item = qobject_cast<ChatItemBase*>(w)) {
            item->setFontScale(_chatFontScale);
        }
    }
}

void ChatWid::openImageViewer(const QPixmap& pix)
{
    auto* dlg = new ElaDialog(this->window());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("图片查看");
    dlg->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    QVBoxLayout* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);

    QLabel* img = new QLabel(dlg);
    img->setAlignment(Qt::AlignCenter);
    img->setPixmap(pix.scaled(QSize(900, 640), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    lay->addWidget(img, 1);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    ElaPushButton* saveBtn = new ElaPushButton("另存为", dlg);
    saveBtn->setFixedHeight(36);
    saveBtn->setMinimumWidth(100);
    connect(saveBtn, &ElaPushButton::clicked, dlg, [pix, dlg]() {
        QString file = QFileDialog::getSaveFileName(dlg, "保存图片", "image.png",
                                                    "图片 (*.png *.jpg *.jpeg)");
        if (!file.isEmpty()) {
            pix.save(file);
        }
    });
    btnRow->addWidget(saveBtn);
    lay->addLayout(btnRow);

    dlg->resize(qMin(pix.width() + 48, 960), qMin(pix.height() + 130, 800));
    dlg->moveToCenter();
    dlg->show();
}

void ChatWid::showItemContextMenu(const QPoint& globalPos, ChatItemBase* item)
{
    if (item == nullptr || _user_info == nullptr) {
        return;
    }
    const QString text = item->textContent();

    QMenu menu(this);
    QAction* copyAct = menu.addAction(QStringLiteral("复制"));
    QAction* quoteAct = menu.addAction(QStringLiteral("引用"));
    // 图片气泡没有文本：复制/引用置灰
    copyAct->setEnabled(!text.isEmpty());
    quoteAct->setEnabled(!text.isEmpty());
    QMenu* fwdMenu = menu.addMenu(QStringLiteral("转发给"));
    for (auto& f : UserMgr::GetInstance()->GetChatListPerPage()) {
        QAction* a = fwdMenu->addAction(f->DisplayName());
        a->setProperty("touid", f->_uid);
    }
    // 重新生成：仅 AI 会话的最后一条 AI 回复
    QAction* regenAct = nullptr;
    if (_user_info->_uid == -1 && item->GetRole() == ChatRole::Other
        && !ui->chat_data_list->items().isEmpty()
        && item == ui->chat_data_list->items().last()) {
        regenAct = menu.addAction(QStringLiteral("重新生成"));
    }
    menu.addSeparator();
    QAction* delAct = menu.addAction(QStringLiteral("删除"));

    QAction* chosen = menu.exec(globalPos);
    if (chosen == nullptr) {
        return;
    }

    if (chosen == copyAct) {
        QGuiApplication::clipboard()->setText(text);
    }
    else if (chosen == quoteAct) {
        ui->chatEdit->setPlainText(QStringLiteral("「引用」%1").arg(text));
        ui->chatEdit->setFocus();
    }
    else if (regenAct != nullptr && chosen == regenAct) {
        ui->chat_data_list->removeItem(item);
        regenerateAiReply();
    }
    else if (chosen == delAct) {
        ui->chat_data_list->removeItem(item);
    }
    else if (chosen->property("touid").isValid()) {
        // 转发纯文本到目标好友
        QJsonObject msgObj;
        msgObj["msgid"] = QUuid::createUuid().toString();
        msgObj["content"] = text;
        QJsonArray textArray;
        textArray.append(msgObj);
        QJsonObject textObj;
        textObj["text_array"] = textArray;
        textObj["fromuid"] = UserMgr::GetInstance()->GetUid();
        textObj["touid"] = chosen->property("touid").toInt();
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ,
                                                 QJsonDocument(textObj).toJson(QJsonDocument::Compact));
        ADDMSG(ElaMessageBarType::Top, "已转发", this, 1, 2000);
    }
}

void ChatWid::retrySendMessage(ChatItemBase* item)
{
    if (item == nullptr || _user_info == nullptr) {
        return;
    }
    const QString text = item->textContent();
    if (text.isEmpty()) {
        return;
    }
    QJsonObject msgObj;
    QString msgid = QUuid::createUuid().toString();
    msgObj["msgid"] = msgid;
    msgObj["content"] = text;
    QJsonArray textArray;
    textArray.append(msgObj);
    QJsonObject textObj;
    textObj["text_array"] = textArray;
    textObj["fromuid"] = UserMgr::GetInstance()->GetUid();
    textObj["touid"] = _user_info->_uid;
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ,
                                             QJsonDocument(textObj).toJson(QJsonDocument::Compact));
    item->setSendStatus(0);
    _pendingMsgs.insert(msgid, item);
}

void ChatWid::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void ChatWid::dropEvent(QDropEvent* event)
{
    QStringList files;
    for (const QUrl& u : event->mimeData()->urls()) {
        if (!u.toLocalFile().isEmpty()) {
            files << u.toLocalFile();
        }
    }
    if (!files.isEmpty()) {
        ui->chatEdit->insertFileFromUrl(files);
        event->acceptProposedAction();
    }
}

void ChatWid::appendTimeDividerIfNeeded()
{
    QDateTime now = QDateTime::currentDateTime();
    if (_lastMsgTs.isValid() && _lastMsgTs.msecsTo(now) < 5 * 60 * 1000) {
        _lastMsgTs = now;
        return;
    }
    _lastMsgTs = now;

    // 居中灰字时间分组
    ElaText* divider = new ElaText(now.toString("HH:mm"), this);
    divider->setWordWrap(false);
    divider->setTextPixelSize(11);
    divider->setAlignment(Qt::AlignCenter);
    divider->setStyleSheet("color: rgba(128, 128, 128, 0.9);");
    ui->chat_data_list->appendItem(divider);
}

// 从 AI 的内存历史（FriendInfo(-1)._chat_msgs，两条管线都会写入）构建请求上下文
QVector<AiMessage> ChatWid::buildAiHistory(int endExclusive)
{
    QVector<AiMessage> history;
    auto aiInfo = UserMgr::GetInstance()->GetFriendById(-1);
    if (!aiInfo) {
        return history;
    }
    const int myUid = UserMgr::GetInstance()->GetUid();
    const int kMaxHistory = 20; // 只带最近 20 条，控制请求体积
    const auto& msgs = aiInfo->_chat_msgs;
    const int total = (endExclusive < 0) ? static_cast<int>(msgs.size())
                                         : qMin(endExclusive, static_cast<int>(msgs.size()));
    for (int i = qMax(0, total - kMaxHistory); i < total; ++i) {
        AiMessage am;
        am.role = (msgs[i]->_from_uid == myUid) ? QStringLiteral("user")
                                                : QStringLiteral("assistant");
        am.content = msgs[i]->_msg_content;
        history.append(am);
    }
    return history;
}

void ChatWid::slot_ai_reply(const QString& text)
{
    removeAiPlaceholder();
    const int summaryTarget = _aiSummaryTargetUid;
    _aiSummaryTargetUid = 0;
    const int myUid = UserMgr::GetInstance()->GetUid();
    QJsonObject obj;
    obj["msgid"] = QUuid::createUuid().toString();
    obj["content"] = text;
    QJsonArray arr;
    arr.append(obj);

    if (summaryTarget != 0) {
        // 落点交给 ChatDialog 按 targetUid 记账，避免视图/记账两个真源不一致
        auto data = std::make_shared<TextChatData>(obj["msgid"].toString(), text, -1, myUid);
        emit sig_ai_summary_reply(summaryTarget, data);
        return;
    }

    // 本地回环：走与服务端推送相同的处理管线（会话列表/气泡/历史一次更新）
    auto msg = std::make_shared<TextChatMsg>(-1, myUid, arr);
    emit TcpMgr::GetInstance()->sig_text_chat_msg(msg);
}

void ChatWid::slot_ai_error(const QString& reason)
{
    _aiSummaryTargetUid = 0;
    removeAiPlaceholder();
    ADDMSG(ElaMessageBarType::Top, reason, this, 0, 3000);
}

// 删除 AI 对最后一条提问的回复，按该提问重发（右键"重新生成"）
void ChatWid::regenerateAiReply()
{
    if (AiMgr::GetInstance()->IsBusy()) {
        ADDMSG(ElaMessageBarType::Top, "AI 正在生成上一条回复，请稍候", this, 2, 2000);
        return;
    }
    auto aiInfo = UserMgr::GetInstance()->GetFriendById(-1);
    if (aiInfo == nullptr) {
        return;
    }
    const int myUid = UserMgr::GetInstance()->GetUid();
    auto& msgs = aiInfo->_chat_msgs;
    int lastUserIdx = -1;
    for (int i = static_cast<int>(msgs.size()) - 1; i >= 0; --i) {
        if (msgs[i]->_from_uid == myUid) {
            lastUserIdx = i;
            break;
        }
    }
    if (lastUserIdx < 0) {
        return;
    }
    // 历史里丢掉这条提问之后的 AI 回复；提问本身保留（视图里它的气泡也在）
    msgs.erase(msgs.begin() + lastUserIdx + 1, msgs.end());
    _aiSummaryTargetUid = 0;
    appendAiPlaceholder();
    AiMgr::GetInstance()->SendChat(buildAiHistory(lastUserIdx), msgs[lastUserIdx]->_msg_content);
}

void ChatWid::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatWid::on_send_btn_clicked()
{
    if(_user_info == nullptr){
        return;
    }
    auto pTextEdit = ui->chatEdit;
    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    // 内容为空（或全是空白）什么也不做：不加时间分割线、不发包
    bool hasContent = false;
    for (const MsgInfo& m : msgList) {
        if (m.msgFlag != "text" || !m.content.trimmed().isEmpty()) {
            hasContent = true;
            break;
        }
    }
    if (!hasContent) {
        return;
    }
    appendTimeDividerIfNeeded();
    const bool isAI = (_user_info->_uid == -1);
    if (isAI && AiMgr::GetInstance()->IsBusy()) {
        ADDMSG(ElaMessageBarType::Top, "AI 正在生成上一条回复，请稍候", this, 2, 2000);
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role = ChatRole::Self;
    QString userIcon = user_info->_icon;

    QJsonObject textObj;
    QJsonArray textArray;
    int txt_size = 0;
    QString aiText; // AI 会话：本轮累计发给模型的纯文本

    for(int i=0; i<msgList.size(); ++i)
    {
        //消息内容长度不合规就跳过
        if(msgList[i].content.length() > 1024){
            continue;
        }

        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserIcon(QPixmap(userIcon));
        pChatItem->setFontScale(_chatFontScale);
        connect(pChatItem, &ChatItemBase::sigContextMenuRequested, this, &ChatWid::showItemContextMenu);
        QWidget *pBubble = nullptr;

        if(type == "text")
        {
            //生成唯一id
            QUuid uuid = QUuid::createUuid();
            //转为字符串
            QString uuidString = uuid.toString();

            pBubble = new TextBubble(role, msgList[i].content);
            if(txt_size + msgList[i].content.length()> 1024){
                textObj["fromuid"] = user_info->_uid;
                textObj["touid"] = _user_info->_uid;
                textObj["text_array"] = textArray;
                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                if (!isAI) {
                    //发送tcp请求给chat server
                    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
                }
            }

            //将bubble和uid绑定，以后可以等网络返回消息后设置是否送达
            //_bubble_map[uuidString] = pBubble;
            txt_size += msgList[i].content.length();
            QJsonObject obj;
            QByteArray utf8Message = msgList[i].content.toUtf8();
            obj["content"] = QString::fromUtf8(utf8Message);
            obj["msgid"] = uuidString;
            textArray.append(obj);
            if (isAI) {
                aiText += (aiText.isEmpty() ? QString() : QStringLiteral("\n")) + msgList[i].content;
            }
            auto txt_msg = std::make_shared<TextChatData>(uuidString, obj["content"].toString(),
                                                          user_info->_uid, _user_info->_uid);
            emit sig_append_send_chat_msg(txt_msg);
            if (!isAI) {
                // 登记待回执气泡（服务端 Defer 必回 ID_TEXT_CHAT_MSG_RSP）
                pChatItem->setSendStatus(0);
                _pendingMsgs.insert(uuidString, pChatItem);
                connect(pChatItem, &ChatItemBase::sigRetrySend, this, &ChatWid::retrySendMessage);
            }
        }
        else if(type == "image")
        {
            auto* picBubble = new PictureBubble(QPixmap(msgList[i].content), role);
            connect(picBubble, &PictureBubble::sigOpenImage, this, &ChatWid::openImageViewer);
            pBubble = picBubble;
        }
        else if(type == "file")
        {

        }
        //发送消息
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            ui->chat_data_list->appendItem(pChatItem);
        }

    }

    //发送给服务器
    textObj["text_array"] = textArray;
    textObj["fromuid"] = user_info->_uid;
    textObj["touid"] = _user_info->_uid;
    QJsonDocument doc(textObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    //发送并清空之前累计的文本列表
    txt_size = 0;
    textArray = QJsonArray();
    textObj = QJsonObject();
    if (isAI) {
        if (aiText.trimmed().isEmpty()) {
            ADDMSG(ElaMessageBarType::Top, "AI 仅支持文本消息", this, 0, 2000);
            return;
        }
        appendAiPlaceholder();
        AiMgr::GetInstance()->SendChat(buildAiHistory(), aiText);
    } else {
        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
    }
}

void ChatWid::on_emo_btn_clicked()
{
    if(!emo_pic){
        emo_pic = new ElaEmojiPicker(this);
        connect(emo_pic, &ElaEmojiPicker::emojiSelected,
            this, [this](const QString& emoji) {
            ui->chatEdit->insertPlainText(emoji);
        });
    }


    QPoint btnPos = ui->emo_btn->mapToGlobal(QPoint(0, 0));
    int btnHeight = ui->emo_btn->height();

    QPoint popupPos = btnPos + QPoint(0, -emo_pic->height());
    QRect screenGeometry = QApplication::primaryScreen()->availableGeometry();
    if (popupPos.y() < screenGeometry.top()) {
        popupPos = btnPos + QPoint(0, btnHeight);
    }

    emo_pic->popup(popupPos);

    QTimer* timer = new QTimer(this);

    connect(timer, &QTimer::timeout, this, [this, timer]() {
        if (!emo_pic || !emo_pic->isVisible()) {
            // 窗口已关闭，恢复按钮状态
            ui->emo_btn->setDown(false);
            ui->emo_btn->clearFocus();
            ui->emo_btn->update();
            ui->chatEdit->setFocus();
            timer->deleteLater();
        }
    });
    timer->start(100);
}

