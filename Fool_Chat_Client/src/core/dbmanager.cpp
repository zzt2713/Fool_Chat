#include "dbmanager.h"
#include <QSettings>
#include <QSqlDriver>
#include <QSqlRecord>

DBManager::DBManager(QObject *parent) : QObject(parent),m_lastError("ces")
{
    QSettings settings("config.ini", QSettings::IniFormat);

    m_host = settings.value("database/host", "localhost").toString();
    m_port = settings.value("database/port", 3308).toInt();
    m_dbName = settings.value("database/database_name", "mhkh").toString();
    m_username = settings.value("database/username", "root").toString();
    m_password = settings.value("database/password").toString();

    // 初始化数据库连接
    initDatabase();
}

DBManager::~DBManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DBManager::initDatabase()
{

    // 获取连接（使用固定的连接名，避免重复创建）
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        m_db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        m_db = QSqlDatabase::addDatabase("QMYSQL");
    }

    m_db.setHostName(m_host);
    m_db.setPort(m_port);
    m_db.setDatabaseName(m_dbName);
    m_db.setUserName(m_username);
    m_db.setPassword(m_password);

    // 设置连接选项
    m_db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=30");

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return true;
}

bool DBManager::isUsernameExist(const QString& username)
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM user WHERE name = :name");
    query.bindValue(":name", username);

    if (!query.exec()) {
        m_lastError = "数据库查询失败";
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        if (count > 0) {
            m_lastError = "该用户名已存在";
            return true;
        }
    }

    return false;
}

bool DBManager::isEmailExist(const QString& email)
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM user WHERE email = :email");
    query.bindValue(":email", email);

    if (!query.exec()) {
        m_lastError = "数据库查询失败";
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        if(count > 0){
            m_lastError = "该邮箱已注册";
            return true;
        }
    }

    return false;
}

bool DBManager::findEmailByUsername(const QString &username, QString &email)
{
    if (!m_db.isOpen()) {
        m_lastError = "数据库未连接";
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT email FROM user WHERE name = :name");
    query.bindValue(":name", username);

    if (!query.exec()) {
        m_lastError = "数据库查询失败: " + query.lastError().text();
        return false;
    }

    if (query.next()) {
        QString dbEmail = query.value("email").toString();

        if (dbEmail.isEmpty()) {
            m_lastError = "该用户未设置邮箱";
            return false;
        }

        if (dbEmail == email) {
            return true;
        } else {
            QString maskedEmail = maskEmail(dbEmail);
            m_lastError = "邮箱错误，您的邮箱是：" + maskedEmail;
            return false;
        }
    } else {
        // 用户名不存在
        m_lastError = "用户名不存在";
        return false;
    }
}
QString DBManager::getEmailByUsername(const QString& username)
{
    if (!m_db.isOpen()) {
        m_lastError = "数据库未连接";
        return QString();
    }

    QSqlQuery query;
    query.prepare("SELECT email FROM user WHERE name = :name");
    query.bindValue(":name", username);

    if (!query.exec()) {
        m_lastError = "数据库查询失败: " + query.lastError().text();
        return QString();
    }

    if (query.next()) {
        QString email = query.value("email").toString();
        if (email.isEmpty()) {
            m_lastError = "该用户未设置邮箱";
        }
        return email;
    } else {
        m_lastError = "用户名不存在";
        return QString();
    }
}

QString DBManager::maskEmail(const QString& email)
{
    if (email.isEmpty()) {
        return QString();
    }

    int atIndex = email.indexOf('@');
    if (atIndex <= 1) {
        // 邮箱格式不正确或用户名部分太短，返回完全脱敏
        return "****" + email.mid(atIndex);
    }

    QString username = email.left(atIndex);
    QString domain = email.mid(atIndex);

    // 根据用户名长度决定显示几个字符
    if (username.length() <= 2) {
        // 用户名很短时，只显示第一个字符，其余用星号
        return username.left(1) + "***" + domain;
    } else if (username.length() <= 4) {
        // 用户名较短时，显示前2个字符
        return username.left(2) + "***" + domain;
    } else {
        // 用户名较长时，显示前3个字符和最后1个字符
        return username.left(3) + "***" + username.right(1) + domain;
    }
}
