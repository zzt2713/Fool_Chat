#ifndef MEDIAENGINE_H
#define MEDIAENGINE_H

#include "F_singleton.h"
#include <QObject>
#include <QString>
#include <memory>

class QVideoWidget;

/**
 * @brief 视频通话媒体层
 * WebRTC 建连（libdatachannel）+ H264/Opus 编解码（ffmpeg/openh264/libopus）
 * + 摄像头/麦克风采集与音视频渲染（Qt Multimedia）
 *
 * 信令本身不在此类内：SDP/ICE 通过 sig_send_sig 抛给 CallManager 转发；
 * 所有公开方法只允许在 GUI 线程调用。
 */
class MediaEngine : public QObject, public F_Singleton<MediaEngine>
{
    Q_OBJECT
public:
    // Impl 为嵌套不完整类型，构造/析构必须在 cpp 中定义（MSVC 展开内联构造需要完整类型）
    MediaEngine();
    ~MediaEngine() override;

    // 创建视频 sink 并连接内部帧分发（幂等，CallManager::init 时调用一次）
    void init();

    // 通话媒体：创建 PeerConnection + 收发 track；asCaller=true 立即生成 offer；
    // callType = CallType::VOICE 时只建音频轨（不采集摄像头）
    void start(bool asCaller, int peerUid, const QString& callId, const QString& callType);
    // 挂断/异常时清理全部媒体资源（幂等）
    void stop();

    // 对端信令入口（GUI 线程）
    void onRemoteOffer(const QString& sdp);
    void onRemoteAnswer(const QString& sdp);
    void onRemoteIce(const QString& candidate);

    // 通话窗口挂接：摄像头喂给本地预览控件并同时采集编码，解码帧推给远端控件
    void attachVideoWidgets(QVideoWidget* local, QVideoWidget* remote);

signals:
    // 需要经 CallManager 转发的信令（offer/answer/ice）
    void sig_send_sig(QString type, QString payload);
    // 连接状态变化（用于通话窗口状态栏）
    void sig_status(QString text);
    // 打洞失败/连接断开，CallManager 收到后主动挂断
    void sig_media_failed();

private:
    friend class F_Singleton<MediaEngine>;

    void setupCamera();
    void setupAudio();
    void flushPendingIce();

    class Impl;
    std::unique_ptr<Impl> _impl;
};

#endif // MEDIAENGINE_H
