#include "MysqlMgr.h"


MysqlMgr::~MysqlMgr() {

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	return _dao.RegUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email) {
	return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name, const std::string& pwd) {
	return _dao.UpdatePwd(name, pwd);
}

MysqlMgr::MysqlMgr() {
}

bool MysqlMgr::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo) {
	return _dao.CheckPwd(name, pwd, userInfo);
}


bool MysqlMgr::AddFriendApply(const int& uid, const int& touid, const std::string& descs)
{
	return _dao.AddFriendApply(uid, touid, descs);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
	return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(std::string name)
{
	return _dao.GetUser(name);
}

bool MysqlMgr::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit)
{
	return _dao.GetApplyList(touid, applyList, begin, limit);
}

bool MysqlMgr::InsertDynamic(int uid, const std::string& content, const std::string& image_urls, int status)
{
	return _dao.InsertDynamic(uid, content, image_urls, status);
}

bool MysqlMgr::GetDynamicList(std::vector<std::shared_ptr<DynamicInfo>>& list, int begin, int limit)
{
	return _dao.GetDynamicList(list, begin, limit);
}

bool MysqlMgr::AuthFriend(int uid, int touid, const std::string& msg)
{
	return _dao.AuthFriend(uid, touid, msg);
}

bool MysqlMgr::UpdateUserStatus(int uid, int status)
{
	return _dao.UpdateUserStatus(uid, status);
}

bool MysqlMgr::GetFriendList(int uid, std::vector<std::shared_ptr<UserInfo>>& user_list)
{
	return _dao.GetFriendList(uid, user_list);
}
bool MysqlMgr::DeleteFriend(const int& uid, const int& touid)
{
	return _dao.DeleteFriend(uid, touid);
}

bool MysqlMgr::GetNoticeList(int uid, std::vector<NoticeInfo>& list)
{
	return _dao.GetNoticeList(uid, list);
}

bool MysqlMgr::MarkNoticeRead(int source, int id)
{
	return _dao.MarkNoticeRead(source, id);
}

bool MysqlMgr::UpdateFriendBack(int self_id, int friend_id, const std::string& back)
{
	return _dao.UpdateFriendBack(self_id, friend_id, back);
}

bool MysqlMgr::UpdateAddPolicy(int uid, int policy)
{
	return _dao.UpdateAddPolicy(uid, policy);
}

int MysqlMgr::GetAddPolicy(int uid)
{
	return _dao.GetAddPolicy(uid);
}
