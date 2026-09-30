#ifndef CONTACTDIALOG_H
#define CONTACTDIALOG_H

#include <QWidget>
#include <QMouseEvent>

#include "../../src/core/usermgr.h"
#include "../../src/core/global.h"
#include "../Chat_Comp/page_base.h"

namespace Ui {
class ContactDialog;
}

class ContactDialog : public Page_Base
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit ContactDialog(QWidget *parent = nullptr);
    ~ContactDialog();
    // 好友被删除：详情页若正显示该好友则切回申请页，并从列表移除
    void slot_remove_contact_user(int uid);
    // 供外部（聊天页头像点击）跳转：详情页展示指定好友
    void ShowFriendInfo(int uid);

private:
    Ui::ContactDialog *ui;
    ChatUIMode _mode;
    ChatUIMode _state;
    bool _b_loading;
    QWidget* _last_widget;
    int _info_uid = -1; // 当前详情页展示的好友 uid

    void ShowSearch(bool bsearch);
    void handleGlobalMousePress(QMouseEvent *);
    void loadMoreConUser();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void slot_text_change(const QString &str);
    void slot_add_apply(std::shared_ptr<AddFriendApply>);
    void slot_loading_contact_user();
    void slot_friend_info_page(std::shared_ptr<UserInfo>);
    void slot_switch_apply_friend_page();
    void slot_jump_chat_item_from_infopage(std::shared_ptr<UserInfo>);
    // 好友列表刷新回包：对账重建联系人与申请列表
    void slot_friend_list_refreshed(bool ok);
signals:
    void sig_jump_chat_item(std::shared_ptr<UserInfo>);
    void sig_switch_to_chat_page();
};

#endif // CONTACTDIALOG_H
