#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr()
{

}

MysqlMgr::MysqlMgr() {

}

int MysqlMgr::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	return dao_.RegUser(name, email, pwd);
}

bool MysqlMgr::CheckEmail(const std::string& name, const std::string& email)
{
	return dao_.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string& name, const std::string& email)
{
	return dao_.UpdatePwd(name, email);
}

bool MysqlMgr::CheckPwd(const std::string& username, const std::string& pwd, UserInfo& userInfo)
{
	return dao_.CheckPwd(username, pwd, userInfo);
}

