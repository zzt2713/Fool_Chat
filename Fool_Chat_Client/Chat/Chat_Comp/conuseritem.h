#ifndef CONUSERITEM_H
#define CONUSERITEM_H

#include <QWidget>
#include "listitembase.h"
#include "userdata.h"

namespace Ui {
class ConUserItem;
}

class ConUserItem : public ListItemBase
{
    Q_OBJECT

public:
    explicit ConUserItem(QWidget *parent = nullptr);
    ~ConUserItem();
    QSize sizeHint() const override;
    void SetInfo(std::shared_ptr<AuthInfo> auth_info);
    void SetInfo(std::shared_ptr<AuthRsp> auth_info);
    void SetInfo(std::shared_ptr<FriendInfo> friend_info);
    void SetInfo(int uid, QString name,QString icon);
    std::shared_ptr<UserInfo> GetInfo();
    void ShowRedPoint(bool show = false);
    void addNewFriend();
    // 供列表实时刷新：同步内存状态并更新状态点
    void SetStatus(int status);
    // 刷新显示名（备注/昵称变化后）
    void RefreshDisplay();
private:
    // 在线状态点：status==1 在线，AI 恒在线，其余离线
    void updateStatus(int status);

    Ui::ConUserItem *ui;
    std::shared_ptr<UserInfo> _info;
};

#endif // CONUSERITEM_H
