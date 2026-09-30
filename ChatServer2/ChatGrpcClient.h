#pragma once

#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include <grpcpp/grpcpp.h> 
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <queue>
#include "data.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "MysqlMgr.h"
#include "RedisMgr.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;
using message::DeleteFriendReq;
using message::DeleteFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::GetChatServerRsp;
using message::LoginRsp;
using message::LoginReq;
using message::ChatService;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

using message::FriendStatusReq;
using message::FriendStatusRsp;

using message::CallSigMsg;
using message::CallSigRsp;

class ChatConPool
{
public:
	ChatConPool(size_t poolSize, std::string host, std::string port);
	~ChatConPool();
	void Close();
	void returnConnection(std::unique_ptr<ChatService::Stub> context);
	std::unique_ptr<ChatService::Stub> getConnection();

private:
	atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	std::mutex mutex_;
	std::queue<std::unique_ptr<ChatService::Stub> > connections_;
	std::condition_variable cond_;
};

class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient();
	AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq& req);
	DeleteFriendRsp NotifyDeleteFriend(std::string server_ip, const DeleteFriendReq& req);
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& req);
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& req, const Json::Value& rtvalue);
	FriendStatusRsp NotifyFriendStatus(std::string server_ip, const FriendStatusReq& req);
	CallSigRsp NotifyCallMsg(std::string server_ip, const CallSigMsg& req);
private:
	ChatGrpcClient();
	unordered_map<std::string, std::unique_ptr<ChatConPool>> _pools;
};

