#include "tcpmgr.h"
#include <QAbstractSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QByteArray>
#include "Logger.h"
#include "usermgr.h"
#include "../Chat/Chat_Comp/userdata.h"
#include "global.h"

TcpMgr::~TcpMgr()
{

}

TcpMgr::TcpMgr():_host(""),_port(0),_b_recv_pending(false),_message_id(0),_message_len(0) {
    QObject::connect(&_socket,&QTcpSocket::connected,[&](){
        // 成功建立连接（重连成功同样走登录重发链路）
        _reconnectDelay = 3000;
        emit sig_con_success(true);
        emit sig_link_state(true);
    });
    QObject::connect(&_socket, &QTcpSocket::readyRead, [&]() {
        _buffer.append(_socket.readAll());
        QDataStream stream(&_buffer, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        forever {
            if(!_b_recv_pending){
                if (_buffer.size() < static_cast<int>(sizeof(quint16) * 2)) {
                    return;
                }
                stream >> _message_id >> _message_len;
                _buffer = _buffer.mid(sizeof(quint16) * 2);
            }
            if(_buffer.size() < _message_len){
                _b_recv_pending = true;
                return;
            }
            _b_recv_pending = false;
            // 读取消息体
            QByteArray messageBody = _buffer.mid(0, _message_len);
            _buffer = _buffer.mid(_message_len);
            handleMsg(ReqId(_message_id),_message_len, messageBody);
        }
    });

    QObject::connect(&_socket, &QTcpSocket::errorOccurred,
        [&](QAbstractSocket::SocketError socketError) {
        Q_UNUSED(socketError);
        LOG_ERROR("TCP socket error: " + _socket.errorString().toStdString());
    });

    QObject::connect(&_socket,&QTcpSocket::disconnected,[&](){
        emit sig_link_state(false);
        // 断线自动重连（指数退避封顶 30s），被踢下线不重连
        if (!_kicked && !_host.isEmpty()) {
            _reconnectTimer->start(_reconnectDelay);
            _reconnectDelay = qMin(_reconnectDelay * 2, 30000);
        }
    });

    _reconnectTimer = new QTimer(this);
    _reconnectTimer->setSingleShot(true);
    QObject::connect(_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!_kicked && _socket.state() == QAbstractSocket::UnconnectedState) {
            _socket.connectToHost(_host, _port);
        }
    });

    QObject::connect(this,&TcpMgr::sig_send_data,this,&TcpMgr::slot_send_data);
    initHandlers();
}

void TcpMgr::initHandlers()
{
    _handlers.insert(ID_CHAT_LOGIN_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            emit sig_login_failed(err);
            return ;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            emit sig_login_failed(err);
            return ;
        }

        auto uid = jsonObj["uid"].toInt();
        auto name = jsonObj["name"].toString();
        auto nick = jsonObj["nick"].toString();
        auto icon = jsonObj["icon"].toString();
        auto sex = jsonObj["sex"].toInt();
        auto user_info = std::make_shared<UserInfo>(uid,name,nick,icon,sex);

        UserMgr::GetInstance()->SetUserInfo(user_info);
        UserMgr::GetInstance()->SetToken(jsonObj["token"].toString());

        if(jsonObj.contains("apply_list")){
            UserMgr::GetInstance()->AppendApplyList(jsonObj["apply_list"].toArray());
        }
        UserMgr::GetInstance()->SetAddPolicy(jsonObj["add_policy"].toInt(1));

        //添加好友列表
        if (jsonObj.contains("friend_list")) {
            UserMgr::GetInstance()->AppendFriendList(jsonObj["friend_list"].toArray());
        }

        // LOG_INFO("用户" + name.toStdString() +"登录成功");
        emit sig_switch_chatdlg();
    });

    _handlers.insert(ID_SEARCH_USER_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            emit sig_user_search(nullptr);
            return ;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            emit sig_user_search(nullptr);
            return ;
        }

        auto search_info = std::make_shared<SearchInfo>(jsonObj["uid"].toInt(),
            jsonObj["name"].toString(),jsonObj["nick"].toString(),
            jsonObj["desc"].toString(),jsonObj["sex"].toInt(),jsonObj["icon"].toString()
        );
        emit sig_user_search(search_info);
    });

    _handlers.insert(ID_ADD_FRIEND_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            int err = ErrorCodes::ERR_JSON;
            return ;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            return ;
        }

        emit sig_add_apply_result(err);
    });

    _handlers.insert(ID_NOTIFY_ADD_FRIEND_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            LOG_ERROR("TCP response missing error field, msg_id=" + std::to_string(id));
            return ;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            return ;
        }

        auto from_uid = jsonObj["applyuid"].toInt();
        QString name = jsonObj["name"].toString();
        QString desc = jsonObj["desc"].toString();
        QString icon = jsonObj["icon"].toString();
        QString nick = jsonObj["nick"].toString();
        int sex = jsonObj["sex"].toInt();

        auto apply_info = std::make_shared<AddFriendApply>(from_uid,name,desc,icon,nick,sex);

        emit sig_friend_apply(apply_info);
    });

    _handlers.insert(ID_NOTIFY_AUTH_FRIEND_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            LOG_ERROR("TCP response missing error field, msg_id=" + std::to_string(id));
            return ;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            return ;
        }

        auto from_uid = jsonObj["applyuid"].toInt();
        QString name = jsonObj["name"].toString();
        QString icon = jsonObj["icon"].toString();
        QString nick = jsonObj["nick"].toString();
        int sex = jsonObj["sex"].toInt();

        auto auth_info = std::make_shared<AuthInfo>(from_uid,name,nick,icon,sex);

        auth_info->_status = jsonObj["status"].toInt();
        emit sig_add_auth_friend(auth_info);
    });

    _handlers.insert(ID_AUTH_FRIEND_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            LOG_ERROR("TCP response missing error field, msg_id=" + std::to_string(id));
            return ;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            return ;
        }

        auto name = jsonObj["name"].toString();
        auto icon = jsonObj["icon"].toString();
        auto nick = jsonObj["nick"].toString();
        auto uid = jsonObj["uid"].toInt();
        auto sex = jsonObj["sex"].toInt();
        auto back = jsonObj["back"].toString();

        auto rsp = std::make_shared<AuthRsp>(uid,name,nick,icon,sex,back);
        rsp->_status = jsonObj["status"].toInt();

        emit sig_auth_rsp(rsp);
    });

    // 动态
    _handlers.insert(ID_PUBLISH_DYNAMIC_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            return;
        }

        emit sig_publish_success();
    });

    _handlers.insert(ID_GET_DYNAMIC_LIST_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            return;
        }

        int err = jsonObj["error"].toInt();
        if(err != ErrorCodes::SUCCESS){
            return;
        }

        emit sig_dynamic_list(jsonObj["dynamic_list"].toArray());
    });

    // 删除好友回复：成功则由 UI 移除条目，失败仅提示
    _handlers.insert(ID_DELETE_FRIEND_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("error")){
            return;
        }
        int err = jsonObj["error"].toInt();
        emit sig_delete_friend_rsp(jsonObj["touid"].toInt(), err);
    });

    // 对方删除好友通知
    _handlers.insert(ID_NOTIFY_DELETE_FRIEND_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("fromuid")){
            return;
        }
        emit sig_delete_friend_notify(jsonObj["fromuid"].toInt());
    });

    // 被顶号/踢下线
    _handlers.insert(ID_NOTIFY_OFF_LINE_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        _kicked = true;
        _reconnectTimer->stop();
        _socket.abort();
        emit sig_kicked();
    });

    // 好友上下线通知
    _handlers.insert(ID_NOTIFY_FRIEND_STATUS_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull()){
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(!jsonObj.contains("uid") || !jsonObj.contains("status")){
            return;
        }
        int uid = jsonObj["uid"].toInt();
        int status = jsonObj["status"].toInt();
        UserMgr::GetInstance()->UpdateFriendStatus(uid, status);
        emit sig_friend_status(uid, status);
    });

    // 通知列表
    _handlers.insert(ID_GET_NOTICES_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull() || !jsonDoc.isObject()){
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        QJsonArray notices = jsonObj["notices"].toArray();
        emit sig_notice_list(notices);
    });

    // 好友备注修改回包：更新内存并通知界面刷新
    _handlers.insert(ID_UPDATE_BACK_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull() || !jsonDoc.isObject()){
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(jsonObj["error"].toInt() != ErrorCodes::SUCCESS){
            return;
        }
        // 回包里 uid 是自己、touid 才是要刷新的好友
        int touid = jsonObj["touid"].toInt();
        QString back = jsonObj["back"].toString();
        UserMgr::GetInstance()->UpdateFriendBack(touid, back);
        emit sig_back_updated(touid, back);
    });

    // 通知已读回包
    _handlers.insert(ID_READ_NOTICE_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull() || !jsonDoc.isObject()){
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        emit sig_notice_read(jsonObj["source"].toInt(), jsonObj["id"].toInt());
    });

    // 文本聊天信息回复：服务端回执 → 按 msgid 刷新送达状态
    _handlers.insert(ID_TEXT_CHAT_MSG_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            LOG_ERROR("TEXT_CHAT_MSG_RSP: invalid JSON payload");
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            LOG_ERROR("TEXT_CHAT_MSG_RSP: missing error field");
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            LOG_ERROR("TEXT_CHAT_MSG_RSP failed, err=" + std::to_string(err));
            return;
        }

        // 送达标记：服务端回执带 text_array（含 msgid），按 msgid 刷新气泡状态
        QStringList msgIds;
        const QJsonArray arrays = jsonObj["text_array"].toArray();
        for (const QJsonValue& v : arrays) {
            msgIds << v.toObject()["msgid"].toString();
        }
        emit sig_msg_delivered(msgIds);
    });

    // 通知用户文本聊天信息
    _handlers.insert(ID_NOTIFY_TEXT_CHAT_MSG_REQ,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        // 将QByteArray转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

        // 检查转换是否成功
        if (jsonDoc.isNull()) {
            LOG_ERROR("NOTIFY_TEXT_CHAT_MSG: invalid JSON payload");
            return;
        }

        QJsonObject jsonObj = jsonDoc.object();

        if (!jsonObj.contains("error")) {
            LOG_ERROR("NOTIFY_TEXT_CHAT_MSG: missing error field");
            return;
        }

        int err = jsonObj["error"].toInt();
        if (err != ErrorCodes::SUCCESS) {
            LOG_ERROR("NOTIFY_TEXT_CHAT_MSG failed, err=" + std::to_string(err));
            return;
        }

        auto msg_ptr = std::make_shared<TextChatMsg>(jsonObj["fromuid"].toInt(),
                                                     jsonObj["touid"].toInt(),jsonObj["text_array"].toArray());
        emit sig_text_chat_msg(msg_ptr);
    });
    // 好友列表刷新回包：合并进 UserMgr 后通知两页重建
    _handlers.insert(ID_GET_FRIEND_LIST_RSP,[this](ReqId id, int len, QByteArray data){
        Q_UNUSED(len);
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isNull() || !jsonDoc.isObject()){
            emit sig_friend_list_refreshed(false);
            return;
        }
        QJsonObject jsonObj = jsonDoc.object();
        if(jsonObj["error"].toInt() != ErrorCodes::SUCCESS){
            emit sig_friend_list_refreshed(false);
            return;
        }
        if(jsonObj.contains("friend_list")){
            UserMgr::GetInstance()->MergeFriendList(jsonObj["friend_list"].toArray());
        }
        if(jsonObj.contains("apply_list")){
            UserMgr::GetInstance()->MergeApplyList(jsonObj["apply_list"].toArray());
        }
        emit sig_friend_list_refreshed(true);
    });
}

void TcpMgr::handleMsg(ReqId id, int len, QByteArray data)
{
    auto iter = _handlers.find(id);
    if(iter == _handlers.end()){
        LOG_WARN("no handler registered for msg_id=" + std::to_string(id));
        return;
    }

    iter.value()(id,len,data);
}

void TcpMgr::logout()
{
    _kicked = true;
    _reconnectTimer->stop();
    _socket.abort();
}

void TcpMgr::slot_tcp_connect(ServerInfo si)
{
    _kicked = false; // 重新登录恢复自动重连
    _host = si.Host;
    _port = static_cast<uint16_t>(si.Port.toUInt());
    _socket.connectToHost(si.Host,_port);

}

void TcpMgr::slot_send_data(ReqId reqId, QByteArray dataBytes)
{
    uint16_t id = reqId;

    // 计算长度（使用网络字节序转换）
    quint16 len = static_cast<quint16>(dataBytes.length());

    // 创建一个QByteArray用于存储要发送的所有数据
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    // 设置数据流使用网络字节序
    out.setByteOrder(QDataStream::BigEndian);

    // 写入ID和长度
    out << id << len;

    // 添加字符串数据
    block.append(dataBytes);

    // 发送数据
    _socket.write(block);
}
