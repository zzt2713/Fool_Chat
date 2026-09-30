#ifndef C_WINDOW_H
#define C_WINDOW_H
/******************************************************************************
*
* @file       c_window.h
* @brief      聊天界面 Function
*
* @author     Fool
* @date       2026/03/08
* @history
*****************************************************************************/

#include "ElaWindow.h"
#include "ElaSuggestBox.h"
#include <QSystemTrayIcon>
#include "Chat_Comp/chatuseritem.h"
#include "Setting_Page/f_setting.h"
#include "Notice_Page/noticepage.h"
#include "Music_Page/musicpage.h"
#include "Chat_Page/chatdialog.h"
#include "Dynamic_Page/dynamic_page.h"
#include "Contact_Page/contactdialog.h"
#include "Editor_Page/editor_page.h"
#include "Chat_Comp/userdata.h"

class C_About;
class NotifyPopup;

class C_Window:public ElaWindow
{
    Q_OBJECT
public:
    explicit C_Window(QWidget* parent = nullptr);
    ~C_Window();

private:
    void initStatus();  // 状态栏
    void initToolBar(); // 工具栏
    void initWindow();  // 初始化窗口
    void initContent(); // 初始化界面内容
    void initClose();  // 关闭窗口
    void initGropMember();    //群成员显示   todo...
    void initGropAnnouncement(); // 群公告显示   todo...
    void initNav(); // 左侧导航
    // 删除好友：两页 UI + 内存统一移除
    void removeFriendUi(int uid);

    template <typename T>
    void setMsgNum(T t,int n);

    F_Setting* _settingPage{nullptr};
    QString _settingKey;
    ElaText* _statusText{nullptr};
    ChatDialog* _chatDialog{nullptr};
    NoticePage* _noticePage{nullptr};
    MusicPage* _musicPage{nullptr};
    Dynamic_Page* _dynamicPage{nullptr};
    QMap<QString, QString> _pageTitleMap;
    ContactDialog* _contactDialog{nullptr};
    EditorPage * _editorPage{nullptr};
    C_About* _aboutPage{nullptr};
    QMap<QString, QString> _aiPageMap; // AI open_page 工具：page名 → 导航节点key
    int _contactMsg;
    QSystemTrayIcon* _trayIcon{nullptr}; // 系统托盘
    int _chatUnread{0};                  // 聊天未读（托盘角标）
    int _noticeUnread{0};                // 通知未读（托盘角标）
    void initTray();                     // 托盘图标 + 菜单
    void updateTrayBadge();              // 未读角标重绘
    void refreshApplyBadge();            // 申请角标按未处理数重算
    // 是否允许弹右下角通知：主窗口非前台（最小化/失焦）才提示
    bool shouldPopupNotice() const;

    NotifyPopup* _notifyPopup{nullptr};  // 右下角消息通知弹窗控制器

public slots:
    void Slot_Set_Msg_Num(QWidget* p,int n);
    void slot_apply_friend(std::shared_ptr<AddFriendApply> apply);
    void slot_switch_to_chat_page();
    // AI 工具执行：open_page / window_minimize / window_center
    void slot_window_op(const QString& op, const QString& arg);
    // 删除好友回包（error 非 0 仅提示）与被删通知
    void slot_delete_friend_rsp(int uid, int error);
    void slot_delete_friend_notify(int fromuid);
    // 右下角通知：新消息 / 对方通过好友申请
    void slot_popup_text_chat_msg(std::shared_ptr<TextChatMsg> msg);
    void slot_popup_add_auth_friend(std::shared_ptr<AuthInfo> auth);
    // 通知卡片点击：激活主窗口并跳转对应页面
    void slot_notice_clicked(int kind, int uid);

Q_SIGNALS:
    Q_SIGNAL void themeButtonClicked();

signals:
    void SigContactApply(std::shared_ptr<AddFriendApply>);
    // 退出登录：通知主窗口回到登录页
    void sigLogoutRequested();

};

#endif // C_WINDOW_H
