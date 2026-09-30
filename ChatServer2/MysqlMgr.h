#pragma once
#include "const.h"
#include "MysqlDao.h"
#include "Singleton.h"
class MysqlMgr: public Singleton<MysqlMgr>
{
	friend class Singleton<MysqlMgr>;
public:
	~MysqlMgr();
	int RegUser(const std::string& name, const std::string& email,  const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string & email);
	bool UpdatePwd(const std::string& name, const std::string& email);
	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
	bool AddFriendApply(const int& uid, const int& touid, const std::string& descs);
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(std::string name);
	bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList,int begin, int limit = 10);
	bool InsertDynamic(int uid, const std::string& content, const std::string& image_urls, int status);
	bool GetDynamicList(std::vector<std::shared_ptr<DynamicInfo>>& list, int begin, int limit = 20);
	bool AuthFriend(int uid, int touid, const std::string& msg);
	bool DeleteFriend(const int& uid, const int& touid);
	bool UpdateUserStatus(int uid, int status);
	bool GetFriendList(int uid, std::vector<std::shared_ptr<UserInfo>>& user_list);
	bool GetNoticeList(int uid, std::vector<NoticeInfo>& list);
	bool MarkNoticeRead(int source, int id);
	bool UpdateFriendBack(int self_id, int friend_id, const std::string& back);
	bool UpdateAddPolicy(int uid, int policy);
	int GetAddPolicy(int uid);

private:
	MysqlMgr();
	MysqlDao  _dao;
};

