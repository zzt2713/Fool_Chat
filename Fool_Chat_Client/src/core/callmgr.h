#ifndef CALLMGR_H
#define CALLMGR_H

#include "F_singleton.h"
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QTimer>

// 通话状态机：Idle → Outgoing/Incoming → Connecting → (回到 Idle)
enum class CallState {
    Idle,       // 无通话
    Outgoing,   // 主叫已发 invite，等待接听
    Incoming,   // 被叫收到 invite，等待用户操作
    Connecting, // 已接听，正在做 WebRTC 协商
};

// 通话结束原因（sig_call_ended 参数）
enum class CallEndReason {
    PeerHangup = 0,  // 对端挂断/结束
    Timeout = 1,     // 主叫 60 秒无人接听
    LocalHangup = 2, // 本机主动挂断
    Disconnected = 3, // TCP 断线被动结束
};

/**
 * @brief 视频通话信令管理器
 * 负责 invite/accept/reject/offer/answer/ice/hangup 的收发与状态机；
 * SDP/ICE 通过 sig_remote_* 透传给媒体层（WebRTC）处理
 */
class CallManager : public QObject, public F_Singleton<CallManager>
{
    Q_OBJECT
public:
    // 连接 TcpMgr 信令信号（幂等，主窗口构建时调用一次）
    void init();

    // 主叫发起通话（type = CallType::VIDEO / VOICE）
    void startCall(int peerUid, const QString& type = QStringLiteral("video"));
    // 被叫接听
    void acceptCall();
    // 被叫拒接
    void rejectCall();
    // 任一方挂断
    void hangupCall();

    CallState state() const { return _state; }
    int peerUid() const { return _peerUid; }
    QString callId() const { return _callId; }
    // 当前通话类型（主叫在 startCall 设置，被叫从 invite 的 payload 解析）
    QString callType() const { return _callType; }
    // 最近一通是否自己发起（决定"通话时长"写入对话时落在哪一侧），reset 不清
    bool asCaller() const { return _asCaller; }

    // 媒体层回传信令（offer/answer/ice）时经此发出，通话中有效
    void sendCallSig(const QString& sigType, const QString& payload = QString());

signals:
    void sig_incoming_call(int fromuid, QString callId); // 被叫：来电
    void sig_call_outgoing(int peerUid);                 // 主叫：呼叫中
    void sig_call_connected(bool asCaller);              // 双方已接听，开始媒体协商
    void sig_call_accepted(QString callId);              // 主叫：对方接听（此后主叫发 offer）
    void sig_call_rejected(int peerUid);                 // 主叫：被拒接（peerUid 供写入对话）
    void sig_rejected_by_self(int peerUid);              // 被叫：本机点了拒接
    void sig_call_offline();                             // 主叫：对方不在线
    // 通话结束；durationSec>0 表示曾接通（用于对话里写"通话时长"）
    void sig_call_ended(int reason, int durationSec, int peerUid);
    void sig_remote_offer(QString sdp);                  // 收到对端 SDP offer
    void sig_remote_answer(QString sdp);                 // 收到对端 SDP answer
    void sig_remote_ice(QString candidate);              // 收到对端 ICE 候选

private:
    friend class F_Singleton<CallManager>;
    CallManager() = default;

    void sendSig(const QString& sigType, const QString& payload = QString());
    static void sendSigTo(int fromUid, int toUid, const QString& callId,
                          const QString& sigType, const QString& payload = QString());
    // 结束通话：先算通话时长（曾接听才有值），再 reset 并广播
    void finishCall(int reason);
    void reset();
    void onSigRsp(int error, const QString& callId);
    void onSigNotify(const QJsonObject& sig);

    bool _inited{false};
    CallState _state{CallState::Idle};
    QString _callId;   // 当前通话 id，同一次通话的信令必须匹配
    QString _callType; // 当前通话类型（video/voice），空视为 video
    bool _asCaller{false}; // 最近一通是否本机发起
    int _peerUid{0};   // 当前对端 uid
    qint64 _acceptedAtMs{0}; // 接听时刻（0=未接听），结束时据此算通话时长
    QTimer* _ringTimer{nullptr}; // 主叫 60s 无人接听自动挂断
};

#endif // CALLMGR_H
