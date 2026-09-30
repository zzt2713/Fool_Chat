#pragma once
#include "Singleton.h"
#include <queue>
#include <thread>
#include "CSession.h"
#include <queue>
#include <map>
#include <functional>
#include "const.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <unordered_map>
#include "data.h"

typedef  function<void(shared_ptr<CSession>, const short &msg_id, const string &msg_data)> FunCallBack;
class LogicSystem:public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void PostMsgToQue(shared_ptr < LogicNode> msg);
private:
	LogicSystem();
	void DealMsg();
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
	bool isPureDigit(const std::string);
	void GetUserByUid(const std::string, Json::Value & );
	void GetUserByName(const std::string, Json::Value& );
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list);
	void RegisterCallBacks();
	void LoginHandler(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data);
	void SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void AddFriend(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void DeleteFriend(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void PublishDynamic(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void GetDynamicList(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void DealCallSigMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void GetNotices(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void ReadNotice(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void UpdateBack(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void SetAddPolicy(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	void RefreshFriendList(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	bool GetFriendList(int uid, std::vector<std::shared_ptr<UserInfo>>& user_list);
	
	std::thread _worker_thread;
	std::queue<shared_ptr<LogicNode>> _msg_que;
	std::mutex _mutex;
	std::condition_variable _consume;
	bool _b_stop;
	std::map<short, FunCallBack> _fun_callbacks;
};

