#ifndef DBMANAGER_H
#define DBMANAGER_H

/******************************************************************************
*
* @file       dbmanager.h
* @brief      数据库查询
*
* @author     Fool
* @date       2026/03/18
* @history
*****************************************************************************/
#include "global.h"
#include "F_singleton.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutex>

class DBManager : public QObject, public F_Singleton<DBManager>
{
    Q_OBJECT
    friend class F_Singleton<DBManager>;
public:
    ~DBManager();

    // 初始化数据库连接
    bool initDatabase();

    // 查询用户是否存在
    bool isUsernameExist(const QString& username);
    bool isEmailExist(const QString& email);
    bool findEmailByUsername(const QString& username, QString& email);
    QString getLastError() const { return m_lastError; }

private:
    DBManager(QObject *parent = nullptr);
    QString getEmailByUsername(const QString& username);
    QString maskEmail(const QString& email);
    // 数据库配置
    QString m_host;
    int m_port;
    QString m_dbName;
    QString m_username;
    QString m_password;

    // 连接对象
    QSqlDatabase m_db;

    // 错误信息
    QString m_lastError;
};

#endif // DBMANAGER_H
