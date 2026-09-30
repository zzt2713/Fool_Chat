#include "usermgr.h"

UserMgr::~UserMgr()
{

}

void UserMgr::SetToken(QString token)
{
    _token = token;
}

int UserMgr::GetUid()
{
    return _user_info->_uid;
}

QString UserMgr::GetName()
{
    return _user_info->_name;
}

QString UserMgr::GetIcon()
{
    return _user_info->_icon;
}

void UserMgr::setIcon(QString icon)
{
    _user_info->_icon =icon;
}

std::vector<std::shared_ptr<ApplyInfo> > UserMgr::GetApplyList()
{
    return _apply_list;
}

bool UserMgr::AlreadyApply(int uid)
{
    // 只拦未处理的同人申请：删好友后对方重新申请要能再次显示
    for(auto it: _apply_list){
        if(it->_uid == uid && it->_status == 0){
            return true;
        }
    }

    return false;
}

void UserMgr::AddApplyList(std::shared_ptr<ApplyInfo> app)
{
    // 同人重新申请：替换旧记录（DB 唯一键也只有一行），避免内存重复
    for (auto& it : _apply_list) {
        if (it->_uid == app->_uid) {
            it = app;
            return;
        }
    }
    _apply_list.push_back(app);
}

int UserMgr::PendingApplyCount() const
{
    int pending = 0;
    for (auto& it : _apply_list) {
        if (it->_status == 0) {
            ++pending;
        }
    }
    return pending;
}

void UserMgr::SetUserInfo(std::shared_ptr<UserInfo> si)
{
    _user_info = si;
}

void UserMgr::AppendApplyList(QJsonArray array)
{
    // 遍历 QJsonArray 并输出每个元素
    for (const QJsonValue &value : array) {
        auto name = value["name"].toString();
        auto desc = value["desc"].toString();
        auto icon = value["icon"].toString();
        auto nick = value["nick"].toString();
        auto sex = value["sex"].toInt();
        auto uid = value["uid"].toInt();
        auto status = value["status"].toInt();
        // 重连重登会再次拉取：已存在的申请跳过
        if (AlreadyApply(uid)) {
            continue;
        }
        auto info = std::make_shared<ApplyInfo>(uid, name,
                                                desc, icon, nick, sex, status);

        _apply_list.push_back(info);
    }
}

void UserMgr::AppendFriendList(QJsonArray array)
{
    for (const QJsonValue& value : array) {
        auto name = value["name"].toString();
        auto desc = value["desc"].toString();
        auto icon = value["icon"].toString();
        auto nick = value["nick"].toString();
        auto sex = value["sex"].toInt();
        auto uid = value["uid"].toInt();
        auto back = value["back"].toString();
        auto status = value["status"].toInt();

        // 重连重登会再次拉取：已存在的好友跳过，避免列表重复
        if (_friend_map.contains(uid)) {
            continue;
        }
        auto info = std::make_shared<FriendInfo>(uid, name,
                                                 nick, icon, sex, desc, back,"",status);
        _friend_list.push_back(info);
        _friend_map.insert(uid, info);
    }
}

// 刷新合并：服务端 friend_list 为权威数据
void UserMgr::MergeFriendList(QJsonArray array)
{
    QSet<int> seen;
    for (const QJsonValue& value : array) {
        auto uid = value["uid"].toInt();
        seen.insert(uid);
        auto find_iter = _friend_map.find(uid);
        if (find_iter != _friend_map.end()) {
            // 原地更新字段，保留 _chat_msgs / _last_msg
            auto& f = find_iter.value();
            f->_name = value["name"].toString();
            f->_nick = value["nick"].toString();
            f->_icon = value["icon"].toString();
            f->_sex = value["sex"].toInt();
            f->_desc = value["desc"].toString();
            f->_back = value["back"].toString();
            f->_status = value["status"].toInt();
        } else {
            auto info = std::make_shared<FriendInfo>(uid,
                value["name"].toString(), value["nick"].toString(),
                value["icon"].toString(), value["sex"].toInt(),
                value["desc"].toString(), value["back"].toString(),
                "", value["status"].toInt());
            _friend_list.push_back(info);
            _friend_map.insert(uid, info);
        }
    }
    // 服务端已不存在的好友：本地移除（AI 的 -1 只在 map 不在 list，不会被扫到）
    for (int i = _friend_list.size() - 1; i >= 0; --i) {
        int uid = _friend_list[i]->_uid;
        if (!seen.contains(uid)) {
            RemoveFriend(uid);
        }
    }
}

// 刷新合并：申请列表只增与更新，已同意/历史条目保留
void UserMgr::MergeApplyList(QJsonArray array)
{
    for (const QJsonValue& value : array) {
        auto uid = value["uid"].toInt();
        bool exist = false;
        for (auto& apply : _apply_list) {
            if (apply->_uid == uid) {
                apply->_name = value["name"].toString();
                apply->_desc = value["desc"].toString();
                apply->_icon = value["icon"].toString();
                apply->_nick = value["nick"].toString();
                apply->_sex = value["sex"].toInt();
                apply->_status = value["status"].toInt();
                exist = true;
                break;
            }
        }
        if (!exist) {
            _apply_list.push_back(std::make_shared<ApplyInfo>(
                uid, value["name"].toString(), value["desc"].toString(),
                value["icon"].toString(), value["nick"].toString(),
                value["sex"].toInt(), value["status"].toInt()));
        }
    }
}

bool UserMgr::CheckFriendById(int uid)
{
    auto iter = _friend_map.find(uid);
    if(iter == _friend_map.end()){
        return false;
    }
    return true;
}

UserMgr::UserMgr():_user_info(nullptr),_chat_loaded(0),_contact_loaded(0)
{

}

void UserMgr::AddFriend(std::shared_ptr<AuthRsp> auth_rsp)
{
    // 重复通知直接忽略：否则 map 换新指针、list 留旧指针，两份数据分叉
    if (_friend_map.contains(auth_rsp->_uid)) {
        return;
    }
    auto friend_info = std::make_shared<FriendInfo>(auth_rsp);
    _friend_map[friend_info->_uid] = friend_info;
    // list/map 必须同步：通讯录"补新增"与分页续拉只读 _friend_list，缺了新好友刷新后无法重建
    _friend_list.push_back(friend_info);
}

void UserMgr::AddFriend(std::shared_ptr<AuthInfo> auth_info)
{
    if (_friend_map.contains(auth_info->_uid)) {
        return;
    }
    auto friend_info = std::make_shared<FriendInfo>(auth_info);
    _friend_map[friend_info->_uid] = friend_info;
    _friend_list.push_back(friend_info);
}

std::shared_ptr<FriendInfo> UserMgr::GetFriendById(int uid)
{
    auto find_it = _friend_map.find(uid);
    if(find_it == _friend_map.end()){
        return nullptr;
    }
    return *find_it;
}

std::vector<std::shared_ptr<FriendInfo> > UserMgr::GetChatListPerPage()
{
    std::vector<std::shared_ptr<FriendInfo>> friend_list;
    int begin = _chat_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= (int)_friend_list.size()) {
        return friend_list;
    }

    if (end > (int)_friend_list.size()) {
        friend_list = std::vector<std::shared_ptr<FriendInfo>>(_friend_list.begin() + begin, _friend_list.end());
        return friend_list;
    }

    friend_list = std::vector<std::shared_ptr<FriendInfo>>(_friend_list.begin() + begin, _friend_list.begin()+ end);
    return friend_list;
}

bool UserMgr::IsLoadChatFin()
{
    if (_chat_loaded >= (int)_friend_list.size()) {
        return true;
    }

    return false;
}

void UserMgr::UpdateChatLoadedCount()
{
    int begin = _chat_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= (int)_friend_list.size()) {
        return ;
    }

    if (end > (int)_friend_list.size()) {
        _chat_loaded = _friend_list.size();
        return ;
    }

    _chat_loaded = end;
}

std::vector<std::shared_ptr<FriendInfo> > UserMgr::GetConListPerPage()
{
    std::vector<std::shared_ptr<FriendInfo>> friend_list;
    int begin = _contact_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= (int)_friend_list.size()) {
        return friend_list;
    }

    if (end > (int)_friend_list.size()) {
        friend_list = std::vector<std::shared_ptr<FriendInfo>>(_friend_list.begin() + begin, _friend_list.end());
        return friend_list;
    }


    friend_list = std::vector<std::shared_ptr<FriendInfo>>(_friend_list.begin() + begin, _friend_list.begin() + end);
    return friend_list;
}

void UserMgr::UpdateContactLoadedCount()
{
    int begin = _contact_loaded;
    int end = begin + CHAT_COUNT_PER_PAGE;

    if (begin >= (int)_friend_list.size()) {
        return;
    }

    if (end > (int)_friend_list.size()) {
        _contact_loaded = _friend_list.size();
        return;
    }

    _contact_loaded = end;
}

bool UserMgr::IsLoadConFin()
{
    if (_contact_loaded >= (int)_friend_list.size()) {
        return true;
    }

    return false;
}

std::shared_ptr<UserInfo> UserMgr::GetUserInfo()
{
    return _user_info;
}

void UserMgr::UpdateFriendStatus(int uid, int status)
{
    auto iter = _friend_map.find(uid);
    if (iter == _friend_map.end()) {
        return;
    }
    iter.value()->_status = status;
}

void UserMgr::UpdateFriendBack(int uid, const QString& back)
{
    auto iter = _friend_map.find(uid);
    if (iter == _friend_map.end()) {
        return;
    }
    iter.value()->_back = back;
}

void UserMgr::SetAddPolicy(int policy)
{
    _addPolicy = policy;
}

int UserMgr::GetAddPolicy() const
{
    return _addPolicy;
}



void UserMgr::AppendFriendChatMsg(int friend_id, std::vector<std::shared_ptr<TextChatData>> msgs)
{
    auto find_iter = _friend_map.find(friend_id);
    if(find_iter == _friend_map.end()){
        return;
    }

    find_iter.value()->AppendChatMsgs(msgs);
}

void UserMgr::RegisterAiFriend()
{
    if (_friend_map.contains(-1)) {
        return;
    }
    // 只进 map 不进 list：两页列表分页读 vector，AI 条目由各页自行置顶插入
    _friend_map[-1] = std::make_shared<FriendInfo>(
        -1, "AI助手", "AI助手", ":/icons/image.png", 0,
        "内置智能助手，可以聊天也能操作软件", "", "你好");
}

void UserMgr::RemoveFriend(int uid)
{
    if (uid == -1) {
        return; // AI 不可删
    }
    _friend_map.remove(uid);
    _pinned_uids.remove(uid); // 顺带清置顶标记

    // 服务端已连带删除双方 friend_apply，内存里的申请记录同步清掉
    for (int i = (int)_apply_list.size() - 1; i >= 0; --i) {
        if (_apply_list[i]->_uid == uid) {
            _apply_list.erase(_apply_list.begin() + i);
        }
    }

    for (int i = 0; i < (int)_friend_list.size(); ++i) {
        if (_friend_list[i]->_uid == uid) {
            // 被删下标在游标之前 → 游标左移一格，防止后续分页跳项
            if (i < _chat_loaded) {
                --_chat_loaded;
            }
            if (i < _contact_loaded) {
                --_contact_loaded;
            }
            _friend_list.erase(_friend_list.begin() + i);
            break;
        }
    }
}

bool UserMgr::IsPinned(int uid) const
{
    return _pinned_uids.contains(uid);
}

void UserMgr::SetPinned(int uid, bool pinned)
{
    if (pinned) {
        _pinned_uids.insert(uid);
    } else {
        _pinned_uids.remove(uid);
    }
}

void UserMgr::ClearFriendChatMsg(int uid)
{
    auto find_iter = _friend_map.find(uid);
    if (find_iter == _friend_map.end()) {
        return;
    }
    find_iter.value()->_chat_msgs.clear();
    find_iter.value()->_last_msg.clear();
}

