#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "UserMgr.h"
#include "RedisMgr.h"
#include "ChatGrpcClient.h"

using namespace std;

LogicSystem::LogicSystem():_b_stop(false){
	RegisterCallBacks();
	_worker_thread = std::thread (&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem(){
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(shared_ptr<LogicNode> msg) {
	try {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		_msg_que.push(msg);

		// 从0变为1，通知信号
		if (_msg_que.size() == 1) {
			unique_lk.unlock();
			_consume.notify_one();
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Exception in PostMsgToQue: " << e.what() << std::endl;
		throw;
	}
	catch (...) {
		std::cerr << "Unknown exception in PostMsgToQue" << std::endl;
		throw;
	}
}

void LogicSystem::DealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//判断队列为空则阻塞等待，释放锁
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}

		//判断是否为关闭状态，执行完逻辑后退出循环
		if (_b_stop ) {
			while (!_msg_que.empty()) {
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end()) {
					_msg_que.pop();
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				_msg_que.pop();
			}
			break;
		}

		//如果没有停，继续处理队列中的消息
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks() {
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriend, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_CALL_SIG_REQ] = std::bind(&LogicSystem::DealCallSigMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);


	// 动态注册
	_fun_callbacks[ID_PUBLISH_DYNAMIC_REQ] = std::bind(&LogicSystem::PublishDynamic, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_GET_DYNAMIC_LIST_REQ] = std::bind(&LogicSystem::GetDynamicList, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_GET_NOTICES_REQ] = std::bind(&LogicSystem::GetNotices, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_SET_ADD_POLICY_REQ] = std::bind(&LogicSystem::SetAddPolicy, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_GET_FRIEND_LIST_REQ] = std::bind(&LogicSystem::RefreshFriendList, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_UPDATE_BACK_REQ] = std::bind(&LogicSystem::UpdateBack, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_READ_NOTICE_REQ] = std::bind(&LogicSystem::ReadNotice, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_DELETE_FRIEND_REQ] = std::bind(&LogicSystem::DeleteFriend, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo) {
	//先从redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is  " << userinfo->uid << " name  is "
			<< userinfo->name << " pwd is " << userinfo->pwd << " email is " << userinfo->email << endl;
	}
	else {
		//redis没有则查询mysql
		//查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			return false;
		}

		userinfo = user_info;

		//将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}
	return true;
}

bool LogicSystem::isPureDigit(const std::string str)
{
	for (char c : str) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(const std::string uid_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	//先从redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " pwd is " << pwd << " email is " << email << " icon is " << icon << endl;

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = std::stoi(uid_str);
	//redis没有则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	//将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//返回结果
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}


void LogicSystem::GetUserByName(const std::string name, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	//先从redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		std::cout << "user  uid is  " << uid << " name  is "
			<< name << " pwd is " << pwd << " email is " << email << endl;

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		return;
	}

	//redis没有则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	//将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//返回结果
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list)
{
	// 从Mysql获取申请列表
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short &msg_id, const string &msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	std::cout << "user login uid is  " << uid << " user token  is "
		<< token << endl;

	Json::Value rtvalue;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, MSG_CHAT_LOGIN_RSP);
	});

	// 从redis获取用户token是否正确
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	if (token_value != token) {
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}

	rtvalue["error"] = ErrorCodes::Success;

	//从状态服务器获取token匹配是否准确
	auto rsp = StatusGrpcClient::GetInstance()->Login(uid, root["token"].asString());


	//查询用户信息
	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;

	// 从数据库获取申请列表
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply) {
		for (auto& apply : apply_list) {
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	// 获取好友列表
	std::vector<std::shared_ptr<UserInfo>> friend_list;
		// add-friend policy of this user (default: need verify)
	int add_policy = MysqlMgr::GetInstance()->GetAddPolicy(uid);
	rtvalue["add_policy"] = add_policy;

bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto& friend_ele : friend_list) {
		Json::Value obj;
		obj["uid"] = friend_ele->uid;
		obj["name"] = friend_ele->name;
		obj["nick"] = friend_ele->nick;
		obj["icon"] = friend_ele->icon;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		obj["status"] = friend_ele->status;
		rtvalue["friend_list"].append(obj);
	}

	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");

	// 增加登录数量
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT,server_name);
	int count = 0;
	if (!rd_res.empty()) {
		count = std::stoi(rd_res);
	}
	++count;

	auto count_str = std::to_string(count);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, count_str);

	//session绑定用户uid
	session->SetUserId(uid);
	//为用户设置登录ip server名称
	std::string  ipkey = USERIPPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(ipkey, server_name);
	//uid和session绑定关系,方便以后踢人操作
		// kick: same account already online, notify the old connection to go offline
	auto old_session = UserMgr::GetInstance()->GetSession(uid);
	if (old_session != nullptr && old_session != session) {
		Json::Value kick;
		kick["error"] = ErrorCodes::Success;
		kick["uid"] = uid;
		old_session->Send(kick.toStyledString(), ID_NOTIFY_OFF_LINE_REQ);
		old_session->Close();
	}
UserMgr::GetInstance()->SetUserSession(uid, session);
	MysqlMgr::GetInstance()->UpdateUserStatus(uid, 1);
		UserMgr::GetInstance()->BroadcastFriendStatus(uid, 1);
	return;
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	std::cout << "user SearchInfo uid is  " << uid_str << endl;

	Json::Value rtvalue;

	Defer deder([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP);
	});

	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		GetUserByUid(uid_str, rtvalue);
	}
	else {
		GetUserByName(uid_str, rtvalue);
	}
}

void LogicSystem::AddFriend(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto applyname = root["applyname"].asString();
	auto bakname = root["bakname"].asString();
	auto desc = root["desc"].asString();
	auto touid = root["touid"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer deder([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_FRIEND_RSP);
	});

	// 写入数据库
		// target add policy: 0 refuse / 1 need verify / 2 auto accept (stored in redis)
	int policy = MysqlMgr::GetInstance()->GetAddPolicy(touid);
	if (policy == 0) {
		rtvalue["error"] = ErrorCodes::AddPolicyRefused;
		return;
	}
	if (policy == 2) {
		// auto accept: AuthFriend expects (accepter, applicant)
		MysqlMgr::GetInstance()->AuthFriend(touid, uid, bakname);

		// notify both sides so lists update without relogin
		auto& pCfg = ConfigMgr::Inst();
		auto pSelf = pCfg["SelfServer"]["Name"];
		auto notifyAuth = [&](int target, int peer) {
			std::string ip;
			if (!RedisMgr::GetInstance()->Get(USERIPPREFIX + std::to_string(target), ip) || ip.empty()) {
				return;
			}
			auto peer_info = std::make_shared<UserInfo>();
			GetBaseInfo(USER_BASE_INFO + std::to_string(peer), peer, peer_info);
			if (ip == pSelf) {
				auto s = UserMgr::GetInstance()->GetSession(target);
				if (s) {
					Json::Value notify;
					notify["error"] = ErrorCodes::Success;
					notify["applyuid"] = peer;
					notify["name"] = peer_info->name;
					notify["nick"] = peer_info->nick;
					notify["icon"] = peer_info->icon;
					notify["sex"] = peer_info->sex;
					notify["status"] = RedisMgr::GetInstance()->ExistsKey(USERIPPREFIX + std::to_string(peer)) ? 1 : 0;
					s->Send(notify.toStyledString(), ID_NOTIFY_AUTH_FRIEND_REQ);
				}
				return;
			}
			AuthFriendReq req;
			req.set_fromuid(peer);
			req.set_touid(target);
			ChatGrpcClient::GetInstance()->NotifyAuthFriend(ip, req);
		};
		notifyAuth(uid, touid);
		notifyAuth(touid, uid);
		return;
	}

MysqlMgr::GetInstance()->AddFriendApply(uid, touid, desc);

	// 从redis查询对应ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);
	//直接通知对方添加好友信息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在线则直接发送通知对方
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = applyname;
			notify["desc"] = desc;
			if (b_info) {
				notify["icon"] = apply_info->icon;
				notify["sex"] = apply_info->sex;
				notify["nick"] = apply_info->nick;
				notify["status"] = RedisMgr::GetInstance()->ExistsKey(USERIPPREFIX + std::to_string(uid)) ? 1 : 0;
			}
			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
		}
		return;
	}

	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);
	add_req.set_name(applyname);
	add_req.set_desc("");
	if (b_info) {
		add_req.set_icon(apply_info->icon);
		add_req.set_sex(apply_info->sex);
		add_req.set_nick(apply_info->nick);
	}

	//跨服通知
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
}

void LogicSystem::PublishDynamic(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto content = root["content"].asString();
	std::string image_urls;
	bool has_images = false;

	if (root.isMember("image_urls") && root["image_urls"].isString()) {
		image_urls = root["image_urls"].asString();
		has_images = !image_urls.empty() && image_urls != "[]" && image_urls != "null";
	}
	else if (root.isMember("images") && root["images"].isArray() && root["images"].size() > 0) {
		Json::FastWriter writer;
		image_urls = writer.write(root["images"]);
		has_images = true;
	}

	auto utf8CharCount = [](const std::string& text) {
		size_t count = 0;
		for (unsigned char ch : text) {
			if ((ch & 0xC0) != 0x80) {
				++count;
			}
		}
		return count;
	};
	int status = (!has_images && utf8CharCount(content) < 100) ? 0 : 1;

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_PUBLISH_DYNAMIC_RSP);
	});

	MysqlMgr::GetInstance()->InsertDynamic(uid, content, image_urls, status);
}

void LogicSystem::GetDynamicList(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto begin = root["begin"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_GET_DYNAMIC_LIST_RSP);
	});

	std::vector<std::shared_ptr<DynamicInfo>> list;
	bool b_ok = MysqlMgr::GetInstance()->GetDynamicList(list, begin, 20);
	if (!b_ok) {
		return;
	}

	for (auto& item : list) {
		Json::Value obj;
		obj["id"] = item->id;
		obj["uid"] = item->uid;
		obj["content"] = item->content;
		obj["like_count"] = item->like_count;
		obj["create_time"] = item->create_time;
		obj["name"] = item->name;
		obj["nick"] = item->nick;
		obj["icon"] = item->icon;
		rtvalue["dynamic_list"].append(obj);
	}
}

void LogicSystem::SetAddPolicy(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto policy = root["policy"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["policy"] = policy;
	Defer defer([this, &rtvalue, session]() {
		session->Send(rtvalue.toStyledString(), ID_SET_ADD_POLICY_RSP);
	});

	if (policy < 0 || policy > 2) {
		rtvalue["error"] = ErrorCodes::Error_Json;
		return;
	}
	MysqlMgr::GetInstance()->UpdateAddPolicy(uid, policy);
}

void LogicSystem::UpdateBack(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["uid"].asInt();
	auto touid = root["touid"].asInt();
	auto back = root["back"].asString();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["touid"] = touid;
	rtvalue["back"] = back;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_UPDATE_BACK_RSP);
	});

	MysqlMgr::GetInstance()->UpdateFriendBack(uid, touid, back);
}

void LogicSystem::GetNotices(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_GET_NOTICES_RSP);
	});

	auto uid = root["uid"].asInt();
	std::vector<NoticeInfo> list;
	bool b_ok = MysqlMgr::GetInstance()->GetNoticeList(uid, list);
	if (!b_ok) {
		return;
	}

	for (auto& item : list) {
		Json::Value obj;
		obj["id"] = item.id;
		obj["source"] = item.source;
		obj["title"] = item.title;
		obj["author"] = item.author;
		obj["content"] = item.content;
		obj["level"] = item.level;
		obj["delivered"] = item.delivered;
		obj["create_time"] = item.create_time;
		rtvalue["notices"].append(obj);
	}
}

void LogicSystem::ReadNotice(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_READ_NOTICE_RSP);
	});

	auto source = root["source"].asInt();
	auto id = root["id"].asInt();
	rtvalue["source"] = source;
	rtvalue["id"] = id;
	MysqlMgr::GetInstance()->MarkNoticeRead(source, id);
}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto back_name = root["back"].asString();
	std::cout << "from " << uid << " auth friend to " << touid << std::endl;

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid);
	bool b_info = GetBaseInfo(base_key, touid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
		rtvalue["uid"] = touid;
		rtvalue["back"] = back_name;
		rtvalue["status"] = RedisMgr::GetInstance()->ExistsKey(USERIPPREFIX + std::to_string(touid)) ? 1 : 0;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}


	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP);
	});

	//更新数据库 添加好友
	MysqlMgr::GetInstance()->AuthFriend(uid, touid, back_name);

	//查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//直接通知对方有认证通过消息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中则直接发送通知对方
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info) {
				notify["name"] = user_info->name;
				notify["nick"] = user_info->nick;
				notify["icon"] = user_info->icon;
				notify["sex"] = user_info->sex;
			}
			else {
				notify["error"] = ErrorCodes::UidInvalid;
			}

			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
		}
		return;
	}

	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);

	//发送通知
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	const Json::Value  arrays = root["text_array"];

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["text_array"] = arrays;
	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
		});


	//查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//直接通知对方有认证通过消息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中则直接发送通知对方
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
		}

		return;
	}


	TextChatMsgReq text_msg_req;
	text_msg_req.set_fromuid(uid);
	text_msg_req.set_touid(touid);
	for (const auto& txt_obj : arrays) {
		auto content = txt_obj["content"].asString();
		auto msgid = txt_obj["msgid"].asString();
		std::cout << "content is " << content << std::endl;
		std::cout << "msgid is " << msgid << std::endl;
		auto* text_msg = text_msg_req.add_textmsgs();
		text_msg->set_msgid(msgid);
		text_msg->set_msgcontent(content);
	}


	//发送通知 todo...
	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}


void LogicSystem::DealCallSigMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto call_id = root["call_id"].asString();
	auto sig_type = root["sig_type"].asString();
	auto payload = root["payload"].asString();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["call_id"] = call_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CALL_SIG_RSP);
		});

	//查询redis 查找touid所在的服务器
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		rtvalue["error"] = ErrorCodes::PeerOffline;
		return;
	}

	Json::Value sig;
	sig["fromuid"] = uid;
	sig["touid"] = touid;
	sig["call_id"] = call_id;
	sig["sig_type"] = sig_type;
	sig["payload"] = payload;

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//对方在本机：直接下发给对方会话
	if (to_ip_value == self_name) {
		auto peer_session = UserMgr::GetInstance()->GetSession(touid);
		if (peer_session) {
			std::string return_str = sig.toStyledString();
			peer_session->Send(return_str, ID_NOTIFY_CALL_SIG_REQ);
		}
		else {
			// uip_ 残留但 session 已失效：回离线错误，不能静默丢信令
			rtvalue["error"] = ErrorCodes::PeerOffline;
			std::cout << "call sig dropped: session null, touid=" << touid
					  << ", type=" << sig_type << std::endl;
		}
		return;
	}

	//跨服转发
	CallSigMsg call_req;
	call_req.set_fromuid(uid);
	call_req.set_touid(touid);
	call_req.set_call_id(call_id);
	call_req.set_sig_type(sig_type);
	call_req.set_payload(payload);
	auto grpc_rsp = ChatGrpcClient::GetInstance()->NotifyCallMsg(to_ip_value, call_req);
	if (grpc_rsp.error() != ErrorCodes::Success) {
		rtvalue["error"] = grpc_rsp.error();
		std::cout << "call sig grpc failed: to=" << to_ip_value
				  << ", touid=" << touid << ", error=" << grpc_rsp.error() << std::endl;
	}
}


void LogicSystem::RefreshFriendList(shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Value rtvalue;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_GET_FRIEND_LIST_RSP);
	});

	// 以会话绑定的 uid 为准，不信任请求体
	int uid = session->GetUserId();
	if (uid <= 0) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	rtvalue["error"] = ErrorCodes::Success;

	// 好友申请列表（与登录回包同构）
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply) {
		for (auto& apply : apply_list) {
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	// 好友列表：在线状态实时查 Redis，不用会话建立时的快照
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	GetFriendList(uid, friend_list);
	for (auto& friend_ele : friend_list) {
		Json::Value obj;
		obj["uid"] = friend_ele->uid;
		obj["name"] = friend_ele->name;
		obj["nick"] = friend_ele->nick;
		obj["icon"] = friend_ele->icon;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		obj["status"] = RedisMgr::GetInstance()->ExistsKey(USERIPPREFIX + std::to_string(friend_ele->uid)) ? 1 : 0;
		rtvalue["friend_list"].append(obj);
	}
}

bool LogicSystem::GetFriendList(int uid, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	return MysqlMgr::GetInstance()->GetFriendList(uid, user_list);
}
void LogicSystem::DeleteFriend(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto touid = root["touid"].asInt();

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["touid"] = touid;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_DELETE_FRIEND_RSP);
	});

	// remove both directional rows from friend table
	MysqlMgr::GetInstance()->DeleteFriend(uid, touid);

	// find peer server via redis uip_
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	// notify peer: local session directly, remote via gRPC
	if (to_ip_value == self_name) {
		auto peer_session = UserMgr::GetInstance()->GetSession(touid);
		if (peer_session) {
			Json::Value notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			std::string return_str = notify.toStyledString();
			peer_session->Send(return_str, ID_NOTIFY_DELETE_FRIEND_REQ);
		}
		return;
	}

	DeleteFriendReq del_req;
	del_req.set_uid(uid);
	del_req.set_touid(touid);
	ChatGrpcClient::GetInstance()->NotifyDeleteFriend(to_ip_value, del_req);
}
