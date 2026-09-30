#pragma once
#include <functional>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <map>
#include <unordered_map>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <cassert>
#include <memory>
#include "Singleton.h"
#include "hiredis.h"

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误
	RPCFailed = 1002,  //RPC请求错误
	VarifyExpired = 1003, //验证码过期
	VarifyCodeErr = 1004, //验证码错误
	UserExist = 1005,       //用户已存在
	PasswdErr = 1006,    //密码错误
	EmailNotMatch = 1007,  //邮箱不匹配
	PasswdUpFailed = 1008,  //更新密码失败
	PasswdInvalid = 1009,   //密码不合法
	TokenInvalid = 1010,   //Token失效
	UidInvalid = 1011,  //uid无效
	AddPolicyRefused = 1012, //target user refuses friend apply
	PeerOffline = 1013, //通话/消息目标用户不在线
};


// Defer类
class Defer {
public:
	// 接受一个lambda表达式或者函数指针
	Defer(std::function<void()> func) : func_(func) {}

	// 析构函数中执行传入的函数
	~Defer() {
		func_();
	}

private:
	std::function<void()> func_;
};

//WebRTC SDP 信令单条可达数 KB，需大于 2KB
#define MAX_LENGTH  1024*8
//头的总长度
#define HEAD_TOTAL_LEN 4
//头的id长度
#define HEAD_ID_LEN 2
//头的数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000


enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登录
	MSG_CHAT_LOGIN_RSP = 1006, //用户登录回包
	ID_SEARCH_USER_REQ = 1007, //用户搜索请求
	ID_SEARCH_USER_RSP = 1008, //搜索用户回包
	ID_ADD_FRIEND_REQ = 1009, //申请添加好友请求
	ID_ADD_FRIEND_RSP = 1010, //申请添加好友回复
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,  //通知用户添加好友申请
	ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求
	ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015, //通知用户认证好友申请
	ID_TEXT_CHAT_MSG_REQ = 1017, //文本聊天消息请求
	ID_TEXT_CHAT_MSG_RSP = 1018, //文本聊天消息回复
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天消息
	ID_NOTIFY_OFF_LINE_REQ = 1021, //通知用户下线
	ID_HEART_BEAT_REQ = 1023,      //心跳请求
	ID_HEARTBEAT_RSP = 1024,       //心跳回复
	ID_PUBLISH_DYNAMIC_REQ = 1025,   //发布动态请求
	ID_PUBLISH_DYNAMIC_RSP = 1026,   //发布动态回复
	ID_GET_DYNAMIC_LIST_REQ = 1027,  //获取动态列表请求
	ID_GET_DYNAMIC_LIST_RSP = 1028,  //获取动态列表回复
	ID_DELETE_FRIEND_REQ = 1031,   //delete friend request (1029/1030 reserved for like)
	ID_DELETE_FRIEND_RSP = 1032,   //delete friend response
	ID_NOTIFY_DELETE_FRIEND_REQ = 1033, //notify peer friend deleted
	ID_NOTIFY_FRIEND_STATUS_REQ = 1035, //notify peer friend online status changed
	ID_GET_NOTICES_REQ = 1036, //get notice list request
	ID_GET_NOTICES_RSP = 1037, //get notice list response
	ID_READ_NOTICE_REQ = 1038, //mark notice read request
	ID_READ_NOTICE_RSP = 1039, //mark notice read response
	ID_UPDATE_BACK_REQ = 1040, //update friend remark request
	ID_UPDATE_BACK_RSP = 1041, //update friend remark response
	ID_SET_ADD_POLICY_REQ = 1042, //set add-friend policy request
	ID_SET_ADD_POLICY_RSP = 1043, //set add-friend policy response
	ID_GET_FRIEND_LIST_REQ = 1044, //获取好友列表请求
	ID_GET_FRIEND_LIST_RSP = 1045, //获取好友列表回包
	ID_CALL_SIG_REQ = 1046, //通话信令请求（invite/accept/reject/offer/answer/ice/hangup）
	ID_CALL_SIG_RSP = 1047, //通话信令回包
	ID_NOTIFY_CALL_SIG_REQ = 1048, //转发给被叫的通话信令
};

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define ADDPOLICYPREFIX  "addpolicy_"
#define LOGIN_COUNT  "logincount"
#define NAME_INFO  "nameinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"

//分布式锁的超时时间
#define LOCK_TIME_OUT 10
//分布式锁的获取时间
#define ACQUIRE_TIME_OUT 5


