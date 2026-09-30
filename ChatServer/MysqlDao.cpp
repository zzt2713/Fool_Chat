#include "MysqlDao.h"
#include "ConfigMgr.h"

MysqlDao::MysqlDao()
{
	auto& cfg = ConfigMgr::Inst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["Port"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];
	pool_.reset(new MySqlPool(host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao() {
	pool_->Close();
}

int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}
		// 准备调用存储过程注册用户
		std::unique_ptr < sql::PreparedStatement > stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
		// 设置参数
		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);

		stmt->execute();
		std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
		if (res->next()) {
			int result = res->getInt("result");
			std::cout << "Result: " << result << std::endl;
			pool_->returnConnection(std::move(con));
			return result;
		}
		pool_->returnConnection(std::move(con));
		return -1;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return -1;
	}
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));
		pstmt->setString(1, name);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next()) {
			std::cout << "Check Email: " << res->getString("email") << std::endl;
			if (email != res->getString("email")) {
				pool_->returnConnection(std::move(con));
				return false;
			}
			pool_->returnConnection(std::move(con));
			return true;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);

		int updateCount = pstmt->executeUpdate();

		std::cout << "Updated rows: " << updateCount << std::endl;
		pool_->returnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::string origin_pwd = "";
		// 查找用户
		while (res->next()) {
			origin_pwd = res->getString("pwd");
			// 打印原始密码用于调试
			std::cout << "Password: " << origin_pwd << std::endl;
			break;
		}

		if (pwd != origin_pwd) {
			return false;
		}
		userInfo.name = name;
		userInfo.email = res->getString("email");
		userInfo.uid = res->getInt("uid");
		userInfo.pwd = origin_pwd;
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}


bool MysqlDao::AddFriendApply(const int& uid, const int& touid, const std::string& descs)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT INTO friend_apply (from_uid, to_uid, descs) VALUES (?,?,?)"
			"ON DUPLICATE KEY UPDATE status = 0, descs = VALUES(descs)"));
		pstmt->setInt(1, uid);
		pstmt->setInt(2, touid);
		pstmt->setString(3, descs);


		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}

		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
	return true;
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// 准备SQL查询
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid); // 设置uid参数

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// 遍历结果雿
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->icon = res->getString("icon");
			user_ptr->uid = uid;
			user_ptr->status = res->getInt("status");
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(std::string name)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	try {
		// 准备SQL查询
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name);

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// 遍历结果雿
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->uid = res->getInt("uid");
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return nullptr;
	}
}

bool MysqlDao::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(
			con->_con->prepareStatement(
				"select apply.from_uid, apply.status, apply.descs, user.name, "
				"user.nick, user.sex, user.desc, user.icon "
				"from friend_apply as apply "
				"join user on apply.from_uid = user.uid "
				"where apply.to_uid = ? "
				"and apply.id > ? "
				"order by apply.id ASC LIMIT ? "
			)
		);

		pstmt->setInt(1, touid); // 设置目标用户uid
		pstmt->setInt(2, begin); // 起始id
		pstmt->setInt(3, limit); // 限制数量
		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// 遍历结果雿
		while (res->next()) {
			auto name = res->getString("name");
			auto uid = res->getInt("from_uid");
			auto status = res->getInt("status");
			auto nick = res->getString("nick");
			auto sex = res->getInt("sex");
			auto desc = res->getString("descs");
			auto icon = res->getString("icon");

			auto apply_ptr = std::make_shared<ApplyInfo>(uid, name, desc, icon, nick, sex, status);

			applyList.push_back(apply_ptr);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

}

bool MysqlDao::InsertDynamic(int uid, const std::string& content, const std::string& image_urls, int status)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"INSERT INTO dynamic (uid, content, image_urls, status) VALUES (?, ?, ?, ?)"));
		pstmt->setInt(1, uid);
		pstmt->setString(2, content);
		pstmt->setString(3, image_urls);
		pstmt->setInt(4, status);

		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}

		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetDynamicList(std::vector<std::shared_ptr<DynamicInfo>>& list, int begin, int limit)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"SELECT d.id, d.uid, d.content, d.like_count, d.create_time, u.name, u.nick, u.icon "
			"FROM dynamic d JOIN user u ON d.uid = u.uid "
			"WHERE d.id > ? AND d.status = 0 ORDER BY d.id DESC LIMIT ?"));

		pstmt->setInt(1, begin);
		pstmt->setInt(2, limit);

		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next()) {
			auto info = std::make_shared<DynamicInfo>();
			info->id = res->getInt("id");
			info->uid = res->getInt("uid");
			info->content = res->getString("content");
			info->like_count = res->getInt("like_count");
			info->create_time = res->getString("create_time");
			info->name = res->getString("name");
			info->nick = res->getString("nick");
			info->icon = res->getString("icon");
			list.push_back(info);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::AuthFriend(int uid, int touid, const std::string& back_name)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		con->_con->setAutoCommit(false);

		// 更新好友申请状怿
		{
			std::unique_ptr<sql::PreparedStatement> pstmt(
				con->_con->prepareStatement(
					"UPDATE friend_apply SET status = 1 "
					"WHERE (from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?)"
				)
			);
			// uid 是同意方，touid 是申请方
			// 原申请记录是：touid -> uid
			pstmt->setInt(1, touid);
			pstmt->setInt(2, uid);
			pstmt->setInt(3, uid);
			pstmt->setInt(4, touid);

			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				con->_con->setAutoCommit(true);
				return false;
			}
		}

		// 插入同意方好友关系：uid -> touid
		{
			std::unique_ptr<sql::PreparedStatement> pstmt(
				con->_con->prepareStatement(
					"INSERT IGNORE INTO friend(self_id, friend_id, back) "
					"VALUES (?, ?, ?)"
				)
			);

			pstmt->setInt(1, uid);
			pstmt->setInt(2, touid);
			pstmt->setString(3, back_name);

			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				con->_con->setAutoCommit(true);
				return false;
			}
		}

		// 插入申请方好友关系：touid -> uid
		{
			std::unique_ptr<sql::PreparedStatement> pstmt(
				con->_con->prepareStatement(
					"INSERT IGNORE INTO friend(self_id, friend_id, back) "
					"VALUES (?, ?, ?)"
				)
			);

			pstmt->setInt(1, touid);
			pstmt->setInt(2, uid);
			pstmt->setString(3, "");

			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				con->_con->setAutoCommit(true);
				return false;
			}
		}

		con->_con->commit();
		con->_con->setAutoCommit(true);

		std::cout << "auth friend success" << std::endl;
		return true;
	}
	catch (sql::SQLException& e) {
		try {
			con->_con->rollback();
			con->_con->setAutoCommit(true);
		}
		catch (...) {}
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetNoticeList(int uid, std::vector<NoticeInfo>& list) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});
	try {
		// StarNotice: all public notices
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"SELECT id, title, author, content, delivered, "
			"DATE_FORMAT(create_time, '%Y-%m-%d %H:%i') AS create_time FROM StarNotice ORDER BY id DESC"));
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		while (res->next()) {
			NoticeInfo info;
			info.id = res->getInt("id");
			info.source = 0;
			info.title = res->getString("title");
			info.author = res->getString("author");
			info.content = res->getString("content");
			info.delivered = res->getInt("delivered");
			info.create_time = res->getString("create_time");
			list.push_back(info);
		}

		// admin_notice: sent to me or broadcast (target_uid IS NULL)
		std::unique_ptr<sql::PreparedStatement> apstmt(con->_con->prepareStatement(
			"SELECT id, title, content, level, delivered, "
			"DATE_FORMAT(create_time, '%Y-%m-%d %H:%i') AS create_time FROM admin_notice "
			"WHERE target_uid IS NULL OR target_uid = ? ORDER BY id DESC"));
		apstmt->setInt(1, uid);
		std::unique_ptr<sql::ResultSet> ares(apstmt->executeQuery());
		while (ares->next()) {
			NoticeInfo info;
			info.id = ares->getInt("id");
			info.source = 1;
			info.title = ares->getString("title");
			info.content = ares->getString("content");
			info.level = ares->getString("level");
			info.delivered = ares->getInt("delivered");
			info.create_time = ares->getString("create_time");
			list.push_back(info);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdateAddPolicy(int uid, int policy) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});
	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"UPDATE user SET add_policy = ? WHERE uid = ?"));
		pstmt->setInt(1, policy);
		pstmt->setInt(2, uid);
		return pstmt->executeUpdate() > 0;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

int MysqlDao::GetAddPolicy(int uid) {
	// default: need verify (1)
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return 1;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});
	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"SELECT add_policy FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		if (res->next()) {
			auto val = res->getString("add_policy");
			if (val.length() > 0) {
				int policy = std::atoi(val.c_str());
				if (policy >= 0 && policy <= 2) {
					return policy;
				}
			}
		}
		return 1;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return 1;
	}
}

bool MysqlDao::UpdateFriendBack(int self_id, int friend_id, const std::string& back) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});
	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"UPDATE friend SET back = ? WHERE self_id = ? AND friend_id = ?"));
		pstmt->setString(1, back);
		pstmt->setInt(2, self_id);
		pstmt->setInt(3, friend_id);
		return pstmt->executeUpdate() > 0;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::MarkNoticeRead(int source, int id) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});
	try {
		const char* sql = (source == 0)
			? "UPDATE StarNotice SET delivered = 1 WHERE id = ?"
			: "UPDATE admin_notice SET delivered = 1 WHERE id = ?";
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(sql));
		pstmt->setInt(1, id);
		return pstmt->executeUpdate() > 0;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::UpdateUserStatus(int uid, int status)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(
			con->_con->prepareStatement("UPDATE user SET status = ? WHERE uid = ?")
		);
		pstmt->setInt(1, status);
		pstmt->setInt(2, uid);
		return pstmt->executeUpdate() >= 0;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_info_list) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});
	try {
		// 准备SQL语句, 根据起始id和限制条数返回列表
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select * from friend where self_id = ? "));

		pstmt->setInt(1, self_id); // 将uid替换为你要查询的uid

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// 遍历结果集
		while (res->next()) {
			auto friend_id = res->getInt("friend_id");
			auto back = res->getString("back");
			//再一次查询friend_id对应的信息
			auto user_info = GetUser(friend_id);
			if (user_info == nullptr) {
				continue;
			}

			user_info->back = back;
			user_info_list.push_back(user_info);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		std::cerr << " (MySQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}

	return true;
}
bool MysqlDao::DeleteFriend(const int& uid, const int& touid)
{
	auto con = pool_->getConnection();
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});

	try {
		if (con == nullptr) {
			return false;
		}

		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"DELETE FROM friend WHERE (self_id = ? AND friend_id = ?) OR (self_id = ? AND friend_id = ?)"));
		pstmt->setInt(1, uid);
		pstmt->setInt(2, touid);
		pstmt->setInt(3, touid);
		pstmt->setInt(4, uid);
		pstmt->execute();

		// also drop both-direction apply records between the two users
		std::unique_ptr<sql::PreparedStatement> apstmt(con->_con->prepareStatement(
			"DELETE FROM friend_apply WHERE (from_uid = ? AND to_uid = ?) OR (from_uid = ? AND to_uid = ?)"));
		apstmt->setInt(1, uid);
		apstmt->setInt(2, touid);
		apstmt->setInt(3, touid);
		apstmt->setInt(4, uid);
		apstmt->execute();
		return true;
	} catch (sql::SQLException& e) {
		std::cerr << "SQLException: " << e.what();
		return false;
	}
}
