#ifndef TCPMGR_H
#define TCPMGR_H

#include "F_singleton.h"
#include <QTcpSocket>
#include "global.h"
#include <functional>
#include <QObject>
#include <QJsonArray>
#include "../Chat/Chat_Comp/userdata.h"

class TcpMgr : public QObject, public F_Singleton<TcpMgr>,public std::enable_shared_from_this<TcpMgr>
{
    Q_OBJECT
public:
    ~TcpMgr();

public:
    // 退出登录：断开连接并禁止自动重连（重新登录会复位）
    void logout();

private:
    friend class F_Singleton<TcpMgr>;
    TcpMgr();
    void initHandlers();
    void handleMsg(ReqId id, int len, QByteArray data);
    QTcpSocket _socket;
    QString _host;
    uint16_t _port;
    QByteArray _buffer; // 收包重组缓冲（偏移量解析，头+body 齐了才消费）
    QMap<ReqId, std::function<void(ReqId id, int len, QByteArray data)>> _handlers;
    QTimer* _reconnectTimer{nullptr}; // 断线自动重连
    bool _kicked{false};              // 被踢下线后不再重连
    int _reconnectDelay{3000};        // 重连退避（毫秒）

public slots:
    void slot_tcp_connect(ServerInfo);
    void slot_send_data(ReqId reqId,QByteArray dataBytes);

signals:
    void sig_con_success(bool bsuccess);
    void sig_send_data(ReqId reqId, QByteArray dataBytes);
    void sig_switch_chatdlg();
    void sig_login_failed(int);
    void sig_user_search(std::shared_ptr<SearchInfo>);
    void sig_friend_apply(std::shared_ptr<AddFriendApply>);
    void sig_dynamic_list(QJsonArray);
    void sig_publish_success();
    void sig_add_auth_friend(std::shared_ptr<AuthInfo>);
    void sig_auth_rsp(std::shared_ptr<AuthRsp>);
    void sig_text_chat_msg(std::shared_ptr<TextChatMsg>);
    void sig_msg_delivered(QStringList msgIds);     // 文本消息服务端回执（按 msgid）
    void sig_back_updated(int uid, QString back);   // 好友备注修改成功（刷新列表）
    void sig_add_apply_result(int error);           // 好友申请发送结果（0成功 其他失败）
    void sig_delete_friend_rsp(int uid, int error); // 删除者回包（error 非0仅提示不动列表）
    void sig_delete_friend_notify(int fromuid);     // 被删者收到通知
    void sig_friend_status(int uid, int status);    // 好友上下线（0离线 1在线）
    void sig_kicked();                              // 被顶号/踢下线
    void sig_link_state(bool online);               // 连接状态（false=断线重连中）
    void sig_notice_list(QJsonArray notices);       // 通知列表
    void sig_notice_read(int source, int id);       // 通知已读回包
    void sig_friend_list_refreshed(bool ok);        // 好友列表刷新回包（true成功）
    void sig_set_policy_rsp(int error);             // 加好友策略保存回包（0成功 其他失败）
    void sig_call_sig_rsp(int error, QString callId);      // 信令回包（error=1013对方不在线）
    void sig_call_sig_notify(QJsonObject sig);            // 收到转发来的通话信令
};

#endif // TCPMGR_H
