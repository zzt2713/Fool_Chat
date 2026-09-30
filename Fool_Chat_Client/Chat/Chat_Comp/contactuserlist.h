#ifndef CONTACTUSERLIST_H
#define CONTACTUSERLIST_H
#include <QListWidget>
#include <QScrollBar>
#include <QEvent>
#include <QWheelEvent>
#include <memory>
#include "userdata.h"
#include "usermgr.h"

class ConUserItem;
class QLabel;

class ContactUserList: public QListWidget
{
    Q_OBJECT
public:
    ContactUserList(QWidget* parent = nullptr);
    void ShowRedPoint(bool bshow = true);
    ~ContactUserList();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void addContactUserList();
    void updateEmptyLabel();
    ConUserItem* _add_friend_item;
    QListWidgetItem * _groupitem;
    bool _load_pending;
    QLabel* _emptyLabel{nullptr}; // 空列表占位提示

public slots:
    void slot_item_clicked(QListWidgetItem *item);
    void slot_add_auth_firend(std::shared_ptr<AuthInfo>);
    void slot_auth_rsp(std::shared_ptr<AuthRsp>);
    // 好友被删除：按 uid 遍历移除条目（AI 的 -1 不处理）
    void slot_remove_contact_user(int uid);
    // 好友上下线：按 uid 遍历刷新条目状态点
    void slot_friend_status(int uid, int status);
    bool hasItemForUid(int uid) const;

signals:
    void sig_loading_contact_user();
    void sig_switch_apply_friend_page();
    void sig_switch_friend_info_page(std::shared_ptr<UserInfo> user_info);
};

#endif // CONTACTUSERLIST_H
