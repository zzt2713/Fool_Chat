#ifndef CHATWID_H
#define CHATWID_H

#include <QWidget>
#include <QPointer>
#include "ElaEmojiPicker.h"
#include "userdata.h"
#include "aimgr.h"

class ChatItemBase;
class TextBubble;
class QTimer;

namespace Ui {
class ChatWid;
}

class ChatWid : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWid(QWidget *parent = nullptr);
    ~ChatWid();
    void SetStatus(bool b_status);
    void SetUserInfo(std::shared_ptr<UserInfo>);
    std::shared_ptr<UserInfo> GetUserInfo() const { return _user_info; }
    // live=true 为会话中实时消息（参与时间分组）；历史回放传 false
    void AppendChatMsg(std::shared_ptr<TextChatData>, bool live = true);

private:
    Ui::ChatWid *ui;
    ElaEmojiPicker * emo_pic{nullptr};
    std::shared_ptr<UserInfo> _user_info;
    QPointer<ChatItemBase> _aiPending; // AI 占位气泡（切会话 removeAllItem 后自动置空）
    QPointer<TextBubble> _aiTypingBubble; // 流式文本气泡（首个增量到达前为空）
    double _chatFontScale{1.0};           // 气泡字体缩放（Ctrl+滚轮）
    QString _aiStreamText;                // AI 流式累计文本
    QTimer* _aiStreamFlushTimer{nullptr}; // 流式渲染节流（50ms 合并一次 Markdown 重排）
    int _aiSummaryTargetUid{0};           // AI 总结目标会话 uid（0=非总结请求，回复走 AI 会话回环）
    QDateTime _lastMsgTs;                 // 上一条消息时间（>5min 插时间分组）
    QMap<QString, QPointer<ChatItemBase>> _pendingMsgs; // msgid → 发送中气泡（送达状态）

    void appendAiPlaceholder();
    void removeAiPlaceholder();
    // endExclusive=-1 取全部历史，否则只取 [0, endExclusive)（重新生成时排除待重发的那条）
    QVector<AiMessage> buildAiHistory(int endExclusive = -1);
    // 重新生成 AI 对上一条提问的回复（删除尾部 AI 回复后重发）
    void regenerateAiReply();
    // Ctrl+滚轮字体缩放：刷新全部气泡字号
    void applyChatFontScale();
    // 图片查看器（点击气泡大图）
    void openImageViewer(const QPixmap& pix);
    // 气泡右键菜单：复制/引用/转发/删除
    void showItemContextMenu(const QPoint& globalPos, ChatItemBase* item);
    // 失败消息重试
    void retrySendMessage(ChatItemBase* item);
    // 距上一条超过 5 分钟时插入时间分组
    void appendTimeDividerIfNeeded();

protected:
    void paintEvent(QPaintEvent *event) override;
    // 聊天区任意位置拖入图片文件 → 进入输入框待发送
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
private slots:
    void on_send_btn_clicked();
    void on_emo_btn_clicked();
    // AI 管理器回调：先清占位，再走本地回环管线
    void slot_ai_reply(const QString& text);
    void slot_ai_error(const QString& reason);

signals:
    void sig_append_send_chat_msg(std::shared_ptr<TextChatData> msg);
    // AI 总结回包：落到 targetUid 会话（视图是否可见由 ChatDialog 按 _cur_chat_uid 判定）
    void sig_ai_summary_reply(int targetUid, std::shared_ptr<TextChatData> msg);
    // 气泡头像点击：请求跳转好友资料页
    void sig_show_profile(int uid);
};

#endif // CHATWID_H
