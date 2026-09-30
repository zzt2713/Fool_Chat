#pragma once
#include "Singleton.h"
#include <memory>
#include <unordered_map>
#include <mutex>

class CSession;
class UserMgr : public Singleton<UserMgr>
{
	friend class Singleton<UserMgr>;
public:
	~UserMgr();
	std::shared_ptr<CSession> GetSession(int uid);
	void SetUserSession(int uid, std::shared_ptr<CSession> session);
	void RmvUserSession(int uid, std::shared_ptr<CSession> session);
	// 上下线广播：通知所有好友自己的在线状态（0离线 1在线）
	void BroadcastFriendStatus(int uid, int status);

private:
	UserMgr();
	std::mutex _session_mtx;
	std::unordered_map<int, std::shared_ptr<CSession>> _uid_to_session;
};

