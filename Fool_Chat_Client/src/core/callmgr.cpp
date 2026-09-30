#include "callmgr.h"
#include "tcpmgr.h"
#include "usermgr.h"
#include "global.h"
#include "mediaengine.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>

void CallManager::init()
{
    if (_inited) {
        return;
    }
    _inited = true;

    auto tcp = TcpMgr::GetInstance();
    connect(tcp.get(), &TcpMgr::sig_call_sig_rsp, this, &CallManager::onSigRsp);
    connect(tcp.get(), &TcpMgr::sig_call_sig_notify, this, &CallManager::onSigNotify);

    _ringTimer = new QTimer(this);
    _ringTimer->setSingleShot(true);
    connect(_ringTimer, &QTimer::timeout, this, [this]() {
        if (_state != CallState::Outgoing) {
            return;
        }
        sendSig(CallSig::HANGUP);
        finishCall(static_cast<int>(CallEndReason::Timeout));
    });

    // ——— 媒体层接线：状态机驱动媒体，媒体信令走本状态机转发 ———
    auto media = MediaEngine::GetInstance();
    media->init();
    connect(this, &CallManager::sig_call_connected, this,
            [this, media](bool asCaller) {
                media->start(asCaller, _peerUid, _callId, _callType);
            });
    connect(this, &CallManager::sig_call_ended, media.get(),
            [media](int) { media->stop(); });
    connect(media.get(), &MediaEngine::sig_send_sig, this,
            [this](QString type, QString payload) { sendCallSig(type, payload); });
    connect(this, &CallManager::sig_remote_offer, media.get(),
            [media](QString sdp) { media->onRemoteOffer(sdp); });
    connect(this, &CallManager::sig_remote_answer, media.get(),
            [media](QString sdp) { media->onRemoteAnswer(sdp); });
    connect(this, &CallManager::sig_remote_ice, media.get(),
            [media](QString candidate) { media->onRemoteIce(candidate); });
    connect(media.get(), &MediaEngine::sig_media_failed, this, [this]() {
        if (_state != CallState::Idle) {
            hangupCall();
        }
    });

    // TCP 断线（掉线/被踢）时直接终结通话，不再发信令；
    // 重连成功时也清一次残留状态，防止漏掉 false 信号导致状态机卡死
    connect(tcp.get(), &TcpMgr::sig_link_state, this, [this](bool online) {
        Q_UNUSED(online);
        if (_state == CallState::Idle) {
            return;
        }
        finishCall(static_cast<int>(CallEndReason::Disconnected));
    });
}

void CallManager::startCall(int peerUid, const QString& type)
{
    if (_state != CallState::Idle || peerUid <= 0) {
        return;
    }
    _peerUid = peerUid;
    _callId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    _callType = (type == CallType::VOICE) ? CallType::VOICE : CallType::VIDEO;
    _asCaller = true;
    _state = CallState::Outgoing;
    // 通话类型放 invite 的 payload，服务端原样透传
    sendSig(CallSig::INVITE, _callType);
    _ringTimer->start(60000);
    emit sig_call_outgoing(peerUid);
}

void CallManager::acceptCall()
{
    if (_state != CallState::Incoming) {
        return;
    }
    _state = CallState::Connecting;
    _asCaller = false;
    _acceptedAtMs = QDateTime::currentMSecsSinceEpoch();
    sendSig(CallSig::ACCEPT);
    emit sig_call_connected(false);
}

void CallManager::rejectCall()
{
    if (_state != CallState::Incoming) {
        return;
    }
    const int peer = _peerUid;
    sendSig(CallSig::REJECT);
    reset();
    emit sig_rejected_by_self(peer);
}

void CallManager::hangupCall()
{
    if (_state == CallState::Idle) {
        return;
    }
    sendSig(CallSig::HANGUP);
    finishCall(static_cast<int>(CallEndReason::LocalHangup));
}

void CallManager::sendSig(const QString& sigType, const QString& payload)
{
    sendSigTo(UserMgr::GetInstance()->GetUid(), _peerUid, _callId, sigType, payload);
}

void CallManager::sendCallSig(const QString& sigType, const QString& payload)
{
    if (_state == CallState::Idle) {
        return;
    }
    sendSig(sigType, payload);
}

void CallManager::sendSigTo(int fromUid, int toUid, const QString& callId,
                            const QString& sigType, const QString& payload)
{
    QJsonObject obj;
    obj["fromuid"] = fromUid;
    obj["touid"] = toUid;
    obj["call_id"] = callId;
    obj["sig_type"] = sigType;
    obj["payload"] = payload;
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CALL_SIG_REQ,
                                              QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CallManager::finishCall(int reason)
{
    int durationSec = 0;
    if (_acceptedAtMs > 0) {
        durationSec =
            static_cast<int>((QDateTime::currentMSecsSinceEpoch() - _acceptedAtMs) / 1000);
    }
    const int peer = _peerUid;
    reset();
    emit sig_call_ended(reason, durationSec, peer);
}

void CallManager::reset()
{
    if (_ringTimer) {
        _ringTimer->stop();
    }
    _state = CallState::Idle;
    _callId.clear();
    _callType.clear();
    _peerUid = 0;
    _acceptedAtMs = 0;
}

void CallManager::onSigRsp(int error, const QString& callId)
{
    Q_UNUSED(callId);
    if (error == 0 || _state != CallState::Outgoing) {
        return;
    }
    // 呼叫阶段任何服务端错误（离线/转发失败）都立即终止，不等 60s 超时
    LOG_ERROR("call invite rsp failed, error=" + std::to_string(error));
    reset();
    // 统一按"打不通"提示，避免把服务端故障误报成"对方拒绝"
    emit sig_call_offline();
}

void CallManager::onSigNotify(const QJsonObject& sig)
{
    const QString type = sig["sig_type"].toString();
    const QString callId = sig["call_id"].toString();
    const int fromUid = sig["fromuid"].toInt();
    const QString payload = sig["payload"].toString();

    if (type == CallSig::INVITE) {
        if (_state != CallState::Idle) {
            // 忙线：直接替当前被叫回一个拒接
            sendSigTo(UserMgr::GetInstance()->GetUid(), fromUid, callId, CallSig::REJECT);
            return;
        }
        _peerUid = fromUid;
        _callId = callId;
        // 类型来自 invite payload；空/异常值回退视频（兼容旧客户端）
        _callType = (payload == CallType::VOICE) ? CallType::VOICE : CallType::VIDEO;
        _state = CallState::Incoming;
        emit sig_incoming_call(fromUid, callId);
        return;
    }

    // 以下信令必须属于当前通话，否则丢弃
    if (_state == CallState::Idle || callId != _callId) {
        return;
    }

    if (type == CallSig::ACCEPT && _state == CallState::Outgoing) {
        if (_ringTimer) {
            _ringTimer->stop();
        }
        _state = CallState::Connecting;
        _acceptedAtMs = QDateTime::currentMSecsSinceEpoch();
        emit sig_call_accepted(callId);
        emit sig_call_connected(true);
    } else if (type == CallSig::REJECT && _state == CallState::Outgoing) {
        const int peer = _peerUid;
        reset();
        emit sig_call_rejected(peer);
    } else if (type == CallSig::HANGUP) {
        finishCall(static_cast<int>(CallEndReason::PeerHangup));
    } else if (type == CallSig::OFFER) {
        emit sig_remote_offer(payload);
    } else if (type == CallSig::ANSWER) {
        emit sig_remote_answer(payload);
    } else if (type == CallSig::ICE) {
        emit sig_remote_ice(payload);
    }
}
