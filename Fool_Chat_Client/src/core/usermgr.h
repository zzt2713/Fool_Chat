#ifndef USERMGR_H
#define USERMGR_H
#include "F_singleton.h"
#include "../Chat/Chat_Comp/userdata.h"
#include <QJsonArray>
#include <QMap>
#include <QSet>
class UserMgr : public QObject,public F_Singleton<UserMgr>,
                public std::enable_shared_from_this<UserMgr>
{
    Q_OBJECT
public:
    friend class F_Singleton<UserMgr>;
    ~UserMgr();
    void SetToken(QString token);
    int GetUid();
    QString GetName();
    QString GetIcon();
    void setIcon(QString icon);
    std::vector<std::shared_ptr<ApplyInfo>> GetApplyList();
    bool AlreadyApply(int uid);
    void AddApplyList(std::shared_ptr<ApplyInfo> app);
    // 未处理申请数（红点角标按此重算，不累加）
    int PendingApplyCount() const;
    void SetUserInfo(std::shared_ptr<UserInfo> si);
    void AppendApplyList(QJsonArray array);
    void AppendFriendList(QJsonArray array);
    // 刷新合并：以服务端为准更新字段、补新增、移除已删除好友（保留聊天记录）
    void MergeFriendList(QJsonArray array);
    // 刷新合并：申请列表只补新与更新，不删除（已同意条目本地保留展示）
    void MergeApplyList(QJsonArray array);
    // 全量好友快照（刷新时对账 UI 用）
    std::vector<std::shared_ptr<FriendInfo>> GetAllFriends() const { return _friend_list; }
    bool CheckFriendById(int uid);
    void AddFriend(std::shared_ptr<AuthRsp> auth_rsp);
    void AddFriend(std::shared_ptr<AuthInfo> auth_info);
    // 注册 AI 机器人（uid=-1）到 map：气泡/历史管线依赖，列表分页读 vector 不受影响
    void RegisterAiFriend();
    // 删除好友（AI 的 -1 不可删）：同时修正两处分页游标防跳项
    void RemoveFriend(int uid);
    // 消息页置顶（仅内存态，本次会话有效）
    bool IsPinned(int uid) const;
    void SetPinned(int uid, bool pinned);
    // 清空与该好友的聊天记录（内存：历史与最后一条消息）
    void ClearFriendChatMsg(int uid);
    std::shared_ptr<FriendInfo> GetFriendById(int uid);
    void AppendFriendChatMsg(int friend_id,std::vector<std::shared_ptr<TextChatData>>);
    std::vector<std::shared_ptr<FriendInfo>> GetChatListPerPage();
    bool IsLoadChatFin();
    void UpdateChatLoadedCount();
    std::vector<std::shared_ptr<FriendInfo>> GetConListPerPage();
    void UpdateContactLoadedCount();
    bool IsLoadConFin();
    std::shared_ptr<UserInfo> GetUserInfo();
    // 好友上下线推送：更新内存中的在线状态
    void UpdateFriendStatus(int uid, int status);
    // 修改好友备注：更新内存中的 back
    void UpdateFriendBack(int uid, const QString& back);
    // 加好友策略：0拒绝 1需验证 2允许（登录时由服务端带回）
    void SetAddPolicy(int policy);
    int GetAddPolicy() const;

private:
    QString _token;
    int _addPolicy{1}; // 加好友策略（0拒绝 1需验证 2允许）
    std::shared_ptr<UserInfo> _user_info{nullptr};
    std::vector<std::shared_ptr<ApplyInfo>> _apply_list;
    QMap<int, std::shared_ptr<FriendInfo>> _friend_map;
    std::vector<std::shared_ptr<FriendInfo>> _friend_list;
    QSet<int> _pinned_uids; // 消息页置顶好友（内存态）


    int _chat_loaded;
    int _contact_loaded;
    UserMgr();
};

#endif // USERMGR_H
