#ifndef CHATITEMBASE_H
#define CHATITEMBASE_H
#include <QWidget>
#include <QGridLayout>
#include "ElaText.h"
#include "global.h"

class BubbleFrame;

class ChatItemBase: public QWidget
{
    Q_OBJECT
public:
    explicit ChatItemBase(ChatRole role, QWidget *parent = nullptr);
    void setUserName(const QString &name);
    void setUserIcon(const QPixmap &icon);
    // 头像归属用户 uid：点击头像跳好友资料页用
    void setUserId(int uid);
    void setWidget(QWidget *w);
    // 气泡文本按比例缩放（Ctrl+滚轮）
    void setFontScale(qreal scale);
    // 发送状态小字：0 发送中 / 1 已送达 / 2 发送失败
    void setSendStatus(int st);

    // 气泡纯文本（右键复制/引用）
    QString textContent() const;
    // 消息归属侧（Self=自己发的 / Other=对方，AI 回复为 Other）
    ChatRole GetRole() const { return _role; }

signals:
    void sigIconClicked(int uid);
    // 右键菜单请求（全局坐标 + 来源条目）
    void sigContextMenuRequested(const QPoint& globalPos, ChatItemBase* item);
    // 点击"发送失败"重试
    void sigRetrySend(ChatItemBase* item);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    ChatRole _role;
    ElaText *_pNameLabel;
    QLabel *_pIconLabel;
    QWidget *_pBubble;
    int _userId{0};
    double _baseFontPt{-1}; // 缩放基准字号（首次设置时记录）
    ElaText* _pStatus{nullptr}; // 发送状态小字
    int _sendStatus{1};         // 当前发送状态（0发送中 1已送达 2失败）
};

#endif // CHATITEMBASE_H
