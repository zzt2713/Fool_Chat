#include "UserMgr.h"
#include "CSession.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "MysqlMgr.h"
#include "ChatGrpcClient.h"

UserMgr::~UserMgr()
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	_uid_to_session.clear();
}

std::shared_ptr<CSession> UserMgr::GetSession(int uid)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	auto iter = _uid_to_session.find(uid);
	if (iter == _uid_to_session.end()) {
		return nullptr;
	}

	return iter->second;
}

void UserMgr::SetUserSession(int uid, std::shared_ptr<CSession> session)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	_uid_to_session[uid] = session;
}

void UserMgr::RmvUserSession(int uid, std::shared_ptr<CSession> session)
{
	if (uid <= 0) {
		return;
	}

	auto uid_str = std::to_string(uid);
	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	bool is_current_session = false;

	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		auto iter = _uid_to_session.find(uid);
		if (iter != _uid_to_session.end() && iter->second == session) {
			_uid_to_session.erase(iter);
			is_current_session = true;
		}
	}

	auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!count_str.empty()) {
		count = std::stoi(count_str);
	}
	if (count > 0) {
		--count;
	}
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, std::to_string(count));

	if (!is_current_session) {
		return;
	}

	std::string ipkey = USERIPPREFIX + uid_str;
	std::string login_server;
	RedisMgr::GetInstance()->Get(ipkey, login_server);
	if (login_server.empty() || login_server == server_name) {
		RedisMgr::GetInstance()->Del(ipkey);
		MysqlMgr::GetInstance()->UpdateUserStatus(uid, 0);
		BroadcastFriendStatus(uid, 0);
	}
}

void UserMgr::BroadcastFriendStatus(int uid, int status)
{
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	if (!MysqlMgr::GetInstance()->GetFriendList(uid, friend_list)) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	for (auto& friend_ele : friend_list) {
		int fid = friend_ele->uid;
		auto fid_str = std::to_string(fid);
		std::string f_server;
		bool b_ip = RedisMgr::GetInstance()->Get(USERIPPREFIX + fid_str, f_server);
		if (!b_ip || f_server.empty()) {
			continue;
		}

		if (f_server == self_name) {
			auto session = GetSession(fid);
			if (session) {
				Json::Value notify;
				notify["error"] = ErrorCodes::Success;
				notify["uid"] = uid;
				notify["status"] = status;
				session->Send(notify.toStyledString(), ID_NOTIFY_FRIEND_STATUS_REQ);
			}
			continue;
		}

		FriendStatusReq req;
		req.set_uid(uid);
		req.set_status(status);
		req.set_touid(fid);
		ChatGrpcClient::GetInstance()->NotifyFriendStatus(f_server, req);
	}
}

UserMgr::UserMgr()
{
}
