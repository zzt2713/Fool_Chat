#pragma once
#include "const.h"
#include "hiredis.h"
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>
#include <chrono>
#include "Singleton.h"

class RedisConPool {
public:
	RedisConPool(size_t poolSize, const char* host, int port, const char* pwd)
		: poolSize_(poolSize), host_(host), port_(port), b_stop_(false), pwd_(pwd){
		for (size_t i = 0; i < poolSize_; ++i) {
			auto* context = createConnection();
			if (context != nullptr) {
				connections_.push(context);
			}
		}

		check_thread_ = std::thread([this]() {
			while (!b_stop_) {
				std::this_thread::sleep_for(std::chrono::seconds(30));
				checkThread();
			}
		});
	}

	~RedisConPool() {
		Close();
		if (check_thread_.joinable()) {
			check_thread_.join();
		}

		std::lock_guard<std::mutex> lock(mutex_);
		while (!connections_.empty()) {
			auto* context = connections_.front();
			redisFree(context);
			connections_.pop();
		}
	}

	redisContext* getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this] {
			if (b_stop_) {
				return true;
			}
			return !connections_.empty();
			});
		//如果停止则直接返回空指针
		if (b_stop_) {
			return  nullptr;
		}
		auto* context = connections_.front();
		connections_.pop();
		return context;
	}

	void returnConnection(redisContext* context) {
		if (context == nullptr) {
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) {
			redisFree(context);
			return;
		}
		connections_.push(context);
		cond_.notify_one();
	}

	void dropConnection(redisContext* context) {
		if (context != nullptr) {
			redisFree(context);
		}

		auto* new_context = createConnection();
		if (new_context == nullptr) {
			return;
		}

		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) {
			redisFree(new_context);
			return;
		}
		connections_.push(new_context);
		cond_.notify_one();
	}

	void Close() {
		b_stop_ = true;
		cond_.notify_all();
	}

private:
	redisContext* createConnection() {
		auto* context = redisConnect(host_.c_str(), port_);
		if (context == nullptr || context->err != 0) {
			if (context != nullptr) {
				std::cout << "Redis connect failed: " << context->errstr << std::endl;
				redisFree(context);
			}
			else {
				std::cout << "Redis connect failed: context is null" << std::endl;
			}
			return nullptr;
		}

		auto* reply = (redisReply*)redisCommand(context, "AUTH %s", pwd_.c_str());
		if (reply == nullptr) {
			std::cout << "Redis auth failed: no reply" << std::endl;
			redisFree(context);
			return nullptr;
		}

		if (reply->type == REDIS_REPLY_ERROR) {
			std::cout << "认证失败" << std::endl;
			freeReplyObject(reply);
			redisFree(context);
			return nullptr;
		}

		freeReplyObject(reply);
		std::cout << "认证成功" << std::endl;
		return context;
	}

	void checkThread() {
		std::queue<redisContext*> idle_connections;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			std::swap(idle_connections, connections_);
		}

		auto pool_size = idle_connections.size();
		for (size_t i = 0; i < pool_size; i++) {
			auto* context = idle_connections.front();
			idle_connections.pop();

			auto* reply = (redisReply*)redisCommand(context, "PING");
			if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
				std::cout << "Redis keepalive failed, reconnecting" << std::endl;
				if (reply != nullptr) {
					freeReplyObject(reply);
				}
				redisFree(context);
				context = createConnection();
			}
			else {
				freeReplyObject(reply);
			}

			if (context != nullptr) {
				std::lock_guard<std::mutex> lock(mutex_);
				if (b_stop_) {
					redisFree(context);
				}
				else {
					connections_.push(context);
					cond_.notify_one();
				}
			}
		}
	}
	std::atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string pwd_;
	int port_;
	std::queue<redisContext*> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::thread  check_thread_;
};

class RedisMgr: public Singleton<RedisMgr>,
	public std::enable_shared_from_this<RedisMgr>
{
	friend class Singleton<RedisMgr>;
public:
	~RedisMgr();
	bool Get(const std::string &key, std::string& value);
	bool Set(const std::string &key, const std::string &value);
	bool LPush(const std::string &key, const std::string &value);
	bool LPop(const std::string &key, std::string& value);
	bool RPush(const std::string& key, const std::string& value);
	bool RPop(const std::string& key, std::string& value);
	bool HSet(const std::string &key, const std::string  &hkey, const std::string &value);
	bool HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen);
	std::string HGet(const std::string &key, const std::string &hkey);
	bool HDel(const std::string& key, const std::string& field);
	bool Del(const std::string &key);
	bool ExistsKey(const std::string &key);
	void Close() {
		_con_pool->Close();
	}
private:
	RedisMgr();
	std::unique_ptr<RedisConPool>  _con_pool;
};