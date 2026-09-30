#pragma once
#include <string>
struct UserInfo {
	UserInfo() :name(""), pwd(""), uid(0), email(""), nick(""), desc(""), sex(0), icon(""), back(""),status(0) {}
	std::string name;
	std::string pwd;
	int uid;
	std::string email;
	std::string nick;
	std::string desc;
	int sex;
	std::string icon;
	std::string back;
	int status;
};

struct ApplyInfo {
	ApplyInfo(int uid, std::string name, std::string desc,
		std::string icon, std::string nick, int sex, int status)
		:_uid(uid), _name(name), _desc(desc),
		_icon(icon), _nick(nick), _sex(sex), _status(status) {
	}

	int _uid;
	std::string _name;
	std::string _desc;
	std::string _icon;
	std::string _nick;
	int _sex;
	int _status;
};

struct DynamicInfo {
	DynamicInfo() : id(0), uid(0), like_count(0) {}
	int id;
	int uid;
	std::string content;
	int like_count;
	std::string create_time;
	std::string name;
	std::string nick;
	std::string icon;
};

struct NoticeInfo {
	NoticeInfo() : id(0), source(0), level(""), delivered(0) {}
	int id;                  // 表主键（标记已读用）
	int source;              // 0=StarNotice 公告 1=admin_notice 管理通知
	std::string title;       // 标题
	std::string author;      // 发布者（admin_notice 为空）
	std::string content;     // 正文
	std::string create_time; // 创建时间
	std::string level;               // 等级
	int delivered;           // 0未读 1已读
};

