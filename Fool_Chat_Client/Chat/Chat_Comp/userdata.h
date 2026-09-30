#ifndef USERDATA_H
#define USERDATA_H
#include <QString>
#include <QMetaType>
#include <QDateTime>
#include <vector>
#include <QJsonArray>
#include <QJsonObject>

class SearchInfo
{
public:
    SearchInfo(int uid, QString name, QString nick, QString desc, int sex, QString icon);
    int _uid;
    QString _name;
    QString _nick;
    QString _desc;
    int _sex;
    QString _icon;

    ~SearchInfo();
};

class AddFriendApply {
public:
    AddFriendApply(int from_uid, QString name, QString desc,
                   QString icon, QString nick, int sex);
    int _from_uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;

};

typedef std::shared_ptr<AddFriendApply> AddFriendApplyPtr;
Q_DECLARE_METATYPE(AddFriendApplyPtr)

struct ApplyInfo {
    ApplyInfo(int uid, QString name, QString desc,
              QString icon, QString nick, int sex, int status)
        :_uid(uid),_name(name),_desc(desc),
        _icon(icon),_nick(nick),_sex(sex),_status(status){}
    ApplyInfo(std::shared_ptr<AddFriendApply> addinfo)
        :_uid(addinfo->_from_uid),_name(addinfo->_name),
        _desc(addinfo->_desc),_icon(addinfo->_icon),
        _nick(addinfo->_nick),_sex(addinfo->_sex),
        _status(0)
    {}

    void SetIcon(QString head){
        _icon = head;
    }
    int _uid;
    QString _name;
    QString _desc;
    QString _icon;
    QString _nick;
    int _sex;
    int _status;
};

struct AuthInfo {
    AuthInfo(int uid, QString name,
             QString nick, QString icon, int sex):
        _uid(uid), _name(name), _nick(nick), _icon(icon),
        _sex(sex){}
    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _status{0}; // 对方实时在线状态：0离线 1在线（认证通知带回）
};

struct AuthRsp {
    AuthRsp(int peer_uid, QString peer_name,
            QString peer_nick, QString peer_icon, int peer_sex,
            QString peer_back = QString())
        :_uid(peer_uid),_name(peer_name),_nick(peer_nick),
        _icon(peer_icon),_sex(peer_sex),_back(peer_back)
    {}

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    QString _back; // 我给对方的备注（认证时填写）
    int _status{0}; // 对方实时在线状态：0离线 1在线（认证回包带回）
};

struct TextChatData;
struct FriendInfo{
    FriendInfo(int uid, QString name, QString nick, QString icon,
               int sex, QString desc, QString back, QString last_msg="",int status = 0):_uid(uid),
        _name(name),_nick(nick),_icon(icon),_sex(sex),_desc(desc),
        _back(back),_last_msg(last_msg),_status(status){}

    FriendInfo(std::shared_ptr<AuthInfo> auth_info):_uid(auth_info->_uid),
        _name(auth_info->_name),_nick(auth_info->_nick),_icon(auth_info->_icon),
        _sex(auth_info->_sex),_status(auth_info->_status){}

    FriendInfo(std::shared_ptr<AuthRsp> auth_rsp):_uid(auth_rsp->_uid),
        _name(auth_rsp->_name),_nick(auth_rsp->_nick),_icon(auth_rsp->_icon),
        _sex(auth_rsp->_sex),_back(auth_rsp->_back),_status(auth_rsp->_status){}

    // 消息
    void AppendChatMsgs(const std::vector<std::shared_ptr<TextChatData>> text_vec);
    // 展示名：好友备注 > 昵称 > 账号名
    QString DisplayName() const;

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    QString _desc;
    QString _back; // 我给该好友的备注（friend.back，空表示未设置）
    QString _last_msg;
    int _status;
    std::vector<std::shared_ptr<TextChatData>> _chat_msgs;
};

struct TextChatData;
struct UserInfo {
    UserInfo(int uid, QString name, QString nick, QString icon, int sex, QString last_msg = ""):
        _uid(uid),_name(name),_nick(nick),_icon(icon),_sex(sex),_last_msg(last_msg){}

    UserInfo(std::shared_ptr<AuthInfo> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),
        _icon(auth->_icon),_sex(auth->_sex),_status(auth->_status),_last_msg(""){}

    UserInfo(int uid, QString name, QString icon):
        _uid(uid), _name(name), _nick(name),_icon(icon),
        _sex(0),_last_msg(""){
    }

    UserInfo(std::shared_ptr<AuthRsp> auth):
        _uid(auth->_uid),_name(auth->_name),_nick(auth->_nick),
        _icon(auth->_icon),_sex(auth->_sex),_back(auth->_back),
        _status(auth->_status),_last_msg(""){}

    UserInfo(std::shared_ptr<SearchInfo> search_info):
        _uid(search_info->_uid),_name(search_info->_name),_nick(search_info->_nick),
        _icon(search_info->_icon),_sex(search_info->_sex),_desc(search_info->_desc),
        _last_msg(""){
    }

    UserInfo(std::shared_ptr<FriendInfo> friend_info):
        _uid(friend_info->_uid),_name(friend_info->_name),_nick(friend_info->_nick),
        _icon(friend_info->_icon),_sex(friend_info->_sex),
        _status(friend_info->_status),_desc(friend_info->_desc),
        _back(friend_info->_back),_last_msg(""){
        _chat_msgs = friend_info->_chat_msgs;
    }
    // 展示名：好友备注 > 昵称 > 账号名
    QString DisplayName() const;

    int _uid;
    QString _name;
    QString _nick;
    QString _icon;
    int _sex;
    int _status{0}; // 在线状态快照（来自 friend_list）：0离线 1在线，仅登录时刷新
    QString _desc;  // 个性签名（user.desc，登录回包带回）
    QString _back; // 我给该好友的备注（friend.back，空表示未设置）
    QString _last_msg;
    std::vector<std::shared_ptr<TextChatData>> _chat_msgs;
};

struct TextChatData{
    TextChatData(QString msg_id, QString msg_content, int fromuid, int touid,
                 qint64 time = -1)
        :_msg_id(msg_id),_msg_content(msg_content),_from_uid(fromuid),_to_uid(touid),
         _time(time >= 0 ? time : QDateTime::currentMSecsSinceEpoch()){

    }
    QString _msg_id;
    QString _msg_content;
    int _from_uid;
    int _to_uid;
    qint64 _time; // 消息时间（毫秒时间戳）：协议带 time 字段时为服务端时间，否则为客户端收到时间
};

struct TextChatMsg{
    TextChatMsg(int fromuid, int touid, QJsonArray arrays):
        _from_uid(fromuid),_to_uid(touid){
        for(auto msg_data : arrays){
            auto msg_obj = msg_data.toObject();
            auto content = msg_obj["content"].toString();
            auto msgid = msg_obj["msgid"].toString();
            const qint64 time = msg_obj.contains("time")
                                     ? static_cast<qint64>(msg_obj["time"].toDouble())
                                     : -1;
            auto msg_ptr = std::make_shared<TextChatData>(msgid, content,fromuid, touid, time);
            _chat_msgs.push_back(msg_ptr);
        }
    }
    int _to_uid;
    int _from_uid;
    std::vector<std::shared_ptr<TextChatData>> _chat_msgs;
};

struct NoticeInfo {
    NoticeInfo(int id, int source, QString title, QString author,
               QString content, QString create_time, QString level, int delivered)
        :_id(id), _source(source), _title(title), _author(author),
        _content(content), _create_time(create_time), _level(level),
        _delivered(delivered){}

    int _id;             // 表主键（标记已读用）
    int _source;         // 0=StarNotice 公告 1=admin_notice 管理通知
    QString _title;      // 标题
    QString _author;     // 发布者（admin_notice 为空）
    QString _content;    // 正文
    QString _create_time;// 创建时间（服务端已格式化）
    QString _level;      // 等级（info/warning/success）
    int _delivered;      // 0未读 1已读
};



#endif // USERDATA_H
