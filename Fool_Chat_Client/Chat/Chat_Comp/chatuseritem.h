#ifndef CHATUSERITEM_H
#define CHATUSERITEM_H

#include <QWidget>
#include "listitembase.h"
#include "userdata.h"

namespace Ui {
class ChatUseritem;
}

class ChatUseritem : public ListItemBase
{
    Q_OBJECT

public:
    explicit ChatUseritem(QWidget *parent = nullptr);
    ~ChatUseritem();
    QSize sizeHint() const override;
    QString getName();
    void SetInfo(QString name,QString head,QString msg);
    void SetInfo(std::shared_ptr<FriendInfo> friend_info);
    void SetInfo(std::shared_ptr<UserInfo> user_info);
    std::shared_ptr<UserInfo> GetUserInfo();
    void updateLastMsg(std::vector<std::shared_ptr<TextChatData>> msgs);
    // 会话红点：count>0 显示，0 隐藏
    void SetUnread(int count);

protected:
    // 头像点击跳好友资料页（拦截 icon_lb 的鼠标按下）
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    // 按快照最后一条消息刷新时间列，无聊天记录则隐藏
    void refreshTimeLabel();

    Ui::ChatUseritem *ui;
    std::shared_ptr<UserInfo> _user_info;
};
#endif // CHATUSERITEM_H

