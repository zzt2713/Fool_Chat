#ifndef GLOBAL_H
#define GLOBAL_H

/******************************************************************************
*
* @file       global.h
* @brief      XXXX Function
*
* @author     Fool
* @date       2026/02/24
* @history
*****************************************************************************/

#include <QWidget>
#include <functional>
#include <QStyle>
#include <memory>
#include <iostream>
#include <mutex>
#include <QByteArray>
#include <QNetworkReply>
#include <QJsonObject>
#include <QDir>
#include <QSettings>
#include <QFile>
#include <QApplication>
#include <QTimer>
#include <QDialog>
#include <QRegularExpression>
#include "core/Logger.h"
/**
 * @brief repolish 刷新qss
 */
extern std::function<void(QWidget*)> repolish;
extern std::function<QString(QString)> hashString;
extern QString NAME;
extern QString EMAIL;

// 登录/注册/找回三页共用的输入校验规则，长度校验由各 check 函数先行单独处理
inline const QRegularExpression PASSWORD_RULE("^[a-zA-Z0-9!@#$%^&*.]{6,15}$");
inline const QRegularExpression EMAIL_RULE(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");

enum ReqId {
    ID_GET_VARIFY_CODE = 1001, // 获取验证码
    ID_REG_USER = 1002, // 注册新用户
    ID_RESET_PWD = 1003, //重置密码
    ID_LOGIN_USER = 1004, //用户登录
    ID_CHAT_LOGIN = 1005, //登陆聊天服务器
    ID_CHAT_LOGIN_RSP= 1006, //登陆聊天服务器回包
    ID_SEARCH_USER_REQ = 1007, //用户搜索请求
    ID_SEARCH_USER_RSP = 1008, //搜索用户回包
    ID_ADD_FRIEND_REQ = 1009,  //添加好友申请
    ID_ADD_FRIEND_RSP = 1010, //申请添加好友回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,  //通知用户添加好友申请
    ID_AUTH_FRIEND_REQ = 1013,  //认证好友请求
    ID_AUTH_FRIEND_RSP = 1014,  //认证好友回复
    ID_NOTIFY_AUTH_FRIEND_REQ = 1015, //通知用户认证好友申请
    ID_TEXT_CHAT_MSG_REQ  = 1017,  //文本聊天信息请求
    ID_TEXT_CHAT_MSG_RSP  = 1018,  //文本聊天信息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息
    ID_NOTIFY_OFF_LINE_REQ = 1021, //通知用户下线
    ID_HEART_BEAT_REQ = 1023,      //心跳请求
    ID_HEARTBEAT_RSP = 1024,       //心跳回复
    ID_PUBLISH_DYNAMIC_REQ = 1025,   //发布动态请求
    ID_PUBLISH_DYNAMIC_RSP = 1026,   //发布动态回复
    ID_GET_DYNAMIC_LIST_REQ = 1027,  //获取动态列表请求
    ID_GET_DYNAMIC_LIST_RSP = 1028,  //获取动态列表回复
    // 1029/1030 预留给点赞（暂缓）
    ID_DELETE_FRIEND_REQ = 1031,     //删除好友请求
    ID_DELETE_FRIEND_RSP = 1032,     //删除好友回复
    ID_NOTIFY_DELETE_FRIEND_REQ = 1033, //通知对方好友已被删除
    ID_MODIFY_PROFILE_REQ = 1034,   //修改个人资料请求
    ID_NOTIFY_FRIEND_STATUS_REQ = 1035, //好友上下线通知
    ID_GET_NOTICES_REQ = 1036,  //获取通知列表请求
    ID_GET_NOTICES_RSP = 1037,  //获取通知列表回复
    ID_READ_NOTICE_REQ = 1038,  //通知标记已读请求
    ID_READ_NOTICE_RSP = 1039,  //通知标记已读回复
    ID_UPDATE_BACK_REQ = 1040,  //修改好友备注请求
    ID_UPDATE_BACK_RSP = 1041,  //修改好友备注回复
    ID_SET_ADD_POLICY_REQ = 1042, //设置加好友策略请求
    ID_SET_ADD_POLICY_RSP = 1043, //设置加好友策略回复
    ID_GET_FRIEND_LIST_REQ = 1044, //获取好友列表请求
    ID_GET_FRIEND_LIST_RSP = 1045, //获取好友列表回包
};

// 服务端错误码补充（与 ChatServer ErrorCodes 对齐）
const int SERVER_ERR_ADD_REFUSED = 1012; // 对方拒绝添加

enum Modules{
    REGISTERMOD = 0,
    RESETMOD = 1,
    LOGINMOD = 2,
    SETTINGMOD = 3,
};

enum ErrorCodes {
    SUCCESS = 0,    //成功
    ERR_JSON = 1,   // json解析错误
    ERR_NETWORK = 2, // 网络错误
};

enum TipErr{
    // 登录注册错误
    TIP_SUCCESS = 0,
    TIP_EMAIL_ERR = 1,
    TIP_PWD_ERR = 2,
    TIP_CONFIRM_ERR = 3,
    TIP_PWD_CONFIRM = 4,
    TIP_VARIFY_ERR = 5,
    TIP_USER_ERR = 6,
    TIP_USER_EXIST = 7,
    TIP_EMAIL_EXIST = 8,
    TIP_USER_NOT_EXIST = 9,
    TIP_EMAIL_NOT_EXIST = 10,
};

enum DBError {
    DB_SUCCESS = 0,
    DB_USER_EXIST = 1,
    DB_EMAIL_EXIST = 2,
    DB_BOTH_EXIST = 3,
    DB_ERROR = -1
};

enum ClickLbState{
    Normal = 0,
    Selected = 1
};

struct ServerInfo{
    QString Host;
    QString Port;
    QString Token;
    int Uid;
};

enum ChatUIMode{
    SearchMode, //搜索模式
    ChatMode, //聊天模式
    ContactMode, //联系模式
    SettingsMode, //设置模式
};

enum ListItemType{
    CHAT_USER_ITEM, //聊天用户
    CONTACT_USER_ITEM, //联系人用户
    SEARCH_USER_ITEM, //搜索到的用户
    ADD_USER_TIP_ITEM, //提示添加用户
    INVALID_ITEM,  //不可点击条目
    GROUP_TIP_ITEM, //分组提示条目
    LINE_ITEM,  //分割线
    APPLY_FRIEND_ITEM, //好友申请
    NOTICE_MSG_ITEM, //消息通知
    DYNAMIC_ITEM, //动态条目
};

enum class ChatRole{
    Self,
    Other
};

struct MsgInfo {
    QString msgFlag;
    QString content;
    QPixmap pixmap;
};

//申请好友标签输入框最低长度
const int MIN_APPLY_LABEL_ED_LEN = 40;

const QString add_prefix = "添加标签 ";

const int  tip_offset = 5;

// 聊天界面加载数量
const int CHAT_COUNT_PER_PAGE = 13;

extern QString gate_url_prefix;


inline std::vector<QString>  str ={"hello world !",
                            "nice to meet u",
                            "New year，new life",
                            "You have to love yourself",
                            "My love is written in the wind ever since the whole world is you"};

inline std::vector<QString> head = {
    ":/res/head_1.jpg",
    ":/res/head_2.jpg",
    ":/res/head_3.jpg",
    ":/res/head_4.jpg",
    ":/res/head_5.jpg"
};

inline std::vector<QString> name = {
    "zzt",
    "fool",
    "wzz",
    "zwz",
};

#endif // GLOBAL_H
