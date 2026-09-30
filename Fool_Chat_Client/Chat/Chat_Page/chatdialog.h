#ifndef CHATDIALOG_H
#define CHATDIALOG_H
/******************************************************************************
*
* @file       chatdialog.h
* @brief      聊天页 Function
*
* @author     Fool
* @date       2026/03/08
* @history
*****************************************************************************/
#include <QWidget>
#include "src/core/global.h"
#include "Chat_Comp/userdata.h"
#include "Chat_Comp/page_base.h"
#include <QListWidgetItem>

namespace Ui {
class ChatDialog;
}

class ChatDialog : public Page_Base
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit ChatDialog(QWidget* parent = nullptr);
    ~ChatDialog();
    void addChatUserList();
    void addAiChat();

private:
    Ui::ChatDialog *ui;
    bool _b_loading;
    ChatUIMode _mode;
    ChatUIMode _state;
    QMap<int, QListWidgetItem*> _chat_items_added;
    int _cur_chat_uid;
    QMap<int, int> _unread_map; // 会话未读条数（uid → 条数，仅内存态）

    void handleGlobalMousePress(QMouseEvent *);
    void ShowSearch(bool bsearch);
    void SetSelectChatItem(int uid = 0);
    void SetSelectChatPage(int uid = 0);
    void loadMoreChatUser();
    void UpdateChatMsg(std::vector<std::shared_ptr<TextChatData>> msgdata);
    // 未读维护：收到消息累加红点，打开会话清除，total 变化时发 sig_chat_unread
    void markUnread(int uid);
    void clearUnread(int uid);
    void refreshUnreadBadge();
    // 置顶后首个普通条目的行号（index0 固定是 AI，其后连续置顶）
    int firstNormalRow() const;
    // 分页加载入列：置顶插置顶区边界，普通追加到末尾
    void placeChatItem(int uid, QListWidgetItem* item);

public:
    // 右键菜单动作（由 ChatUseritem 菜单回调）
    void togglePin(int uid);
    void clearChatHistory(int uid);
    // 头像点击跳好友资料页（由 ChatUseritem/气泡回调）
    void showFriendProfile(int uid);
    // 打开第一个未读会话（导航"消息"节点点击时直达）
    void openFirstUnread();
    // 当前打开会话的 uid（0 = 未选中；通知去重用）
    int currentChatUid() const { return _cur_chat_uid; }
    // 好友列表刷新回包：对账重建会话条目
    void slot_friend_list_refreshed(bool ok);
    // 本地追加一条通话系统消息（拒接/通话时长），不走网络：
    // 模型 + 会话列表预览 + 当前打开的聊天页同步更新
    void AppendLocalCallMsg(int peerUid, bool fromSelf, const QString& text);
    // AI 总结回包：历史/预览按 targetUid 记账，气泡仅目标会话打开时插入
    void slot_ai_summary_reply(int targetUid, std::shared_ptr<TextChatData> msg);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    // 未读总数变化（供主窗口刷新聊天导航节点红点）
    void sig_chat_unread(int total);
    // 请求主窗口跳转通讯录并展示好友资料
    void sig_show_friend_profile(int uid);

private slots:
    void slot_loading_chat_user();
    void slot_text_change(const QString &str);
    void slot_item_clicked(QListWidgetItem* item);

public slots:
    void slot_add_auth_friend(std::shared_ptr<AuthInfo> auth_info);
    void slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp);
    void slot_jump_chat_item(std::shared_ptr<SearchInfo> si);
    void slot_jump_chat_item_from_user_info(std::shared_ptr<UserInfo> user_info);
    void slot_append_send_chat_msg(std::shared_ptr<TextChatData> msg);
    void slot_text_chat_msg(std::shared_ptr<TextChatMsg> msg);
    // 好友被删除：从会话列表移除；删的是当前会话则切回第一行(AI)
    void slot_remove_chat_user(int uid);
};

#endif // CHATDIALOG_H
