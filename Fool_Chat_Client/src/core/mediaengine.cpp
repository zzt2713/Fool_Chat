#include "mediaengine.h"
#include "global.h"
#include "Logger.h"

#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QCamera>
#include <QIODevice>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMutex>
#include <QRandomGenerator>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoWidget>
#include <QVideoSink>
#include <QPointer>
#include <rtc/rtc.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#include <opus/opus.h>

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <vector>

namespace {

constexpr int kAudioSampleRate = 48000;
constexpr int kAudioChannels = 1;
constexpr int kOpusFrameSamples = 960; // 20ms@48k
constexpr int kAudioFrameBytes = kOpusFrameSamples * kAudioChannels * 2; // Int16
constexpr int kVideoPayloadType = 96;
constexpr int kAudioPayloadType = 111;
constexpr int kTargetFps = 15;
constexpr int kMinFrameIntervalMs = 1000 / kTargetFps;
constexpr qint64 kMaxPcmBacklogBytes = kAudioSampleRate * 2 / 2; // 播放积压超过 0.5s 即丢弃

} // namespace

class MediaEngine::Impl
{
public:
    // ——— 通话身份（GUI 线程写，回调线程只读一次）———
    std::atomic<bool> running{false};

    // ——— WebRTC（pcMutex 保护三个指针的跨线程读写：GUI 写 / rtc 回调读）———
    std::mutex pcMutex;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::Track> videoTrack;
    std::shared_ptr<rtc::Track> audioTrack;
    std::mutex pendingIceMutex;
    std::vector<std::string> pendingIce; // 远端 description 未就绪时缓存的候选

    // rtc 回调线程校验归属：当前 pc 是否就是回调发起方，且通话仍在进行
    bool isCurrentPc(const std::weak_ptr<rtc::PeerConnection>& self)
    {
        if (!running.load()) {
            return false;
        }
        std::lock_guard<std::mutex> lk(pcMutex);
        return pc != nullptr && pc == self.lock();
    }

    bool isCurrentTrack(const std::weak_ptr<rtc::Track>& self,
                        bool isVideo)
    {
        if (!running.load()) {
            return false;
        }
        std::lock_guard<std::mutex> lk(pcMutex);
        return isVideo ? (videoTrack != nullptr && videoTrack == self.lock())
                       : (audioTrack != nullptr && audioTrack == self.lock());
    }

    // ——— 视频编码（GUI 线程）———
    AVCodecContext* venc = nullptr;
    SwsContext* swsEnc = nullptr;
    int encW = 0;
    int encH = 0;
    int64_t encPts = 0;
    QElapsedTimer frameThrottle;

    bool voiceOnly = false; // 语音通话：无视频轨、不采摄像头

    // ——— 视频解码（rtc 回调线程，decMutex 保护）———
    std::mutex decMutex;
    AVCodecContext* vdec = nullptr;
    SwsContext* swsDec = nullptr;
    bool gotVideoKeyframe = false; // 首个关键帧前丢弃解码输出，避免接听瞬间花屏

    int cameraWarmupSkips = 2; // 摄像头前两帧可能未就绪（驱动预热），跳过不编码

    // ——— 音频（libopus，内部统一 48k/mono/Int16）———
    OpusEncoder* opusEnc = nullptr;
    OpusDecoder* opusDec = nullptr;
    std::mutex decAudioMutex;
    QByteArray audioAccum;      // 采集侧未满 20ms 的残留
    qint64 samplesSent = 0;     // 已编码采样数（播放/发送时间戳）
    QByteArray pcmOut;          // 解码侧待播放 PCM（48k/mono/Int16）
    QMutex pcmOutMutex;
    QAudioFormat inFmt;         // 麦克风实际格式（输入端点协商结果）
    QAudioFormat outFmt;        // 扬声器实际格式（输出端点独立协商，不能复用输入的）
    SwrContext* swrCapture = nullptr;  // 输入格式 → 48k/mono/Int16
    SwrContext* swrPlayback = nullptr; // 48k/mono/Int16 → 输出格式
    int srcFrameBytes = 0;      // 输入格式下 20ms 的字节数
    int srcSamplesPerFrame = 0; // 输入格式下 20ms 每声道采样数

    // ——— Qt 采集/渲染（通话窗口控件自带的 sink，QPointer 随控件销毁自动置空）———
    QPointer<QVideoSink> localSink;  // 摄像头喂给本地预览控件，同时从这里取帧编码
    QPointer<QVideoSink> remoteSink; // 解码帧推入远端画面控件
    QCamera* camera = nullptr;
    bool cameraFormatRetried = false; // 自定义格式失败后回退默认格式的重试标记
    QMediaCaptureSession* capSession = nullptr;
    QAudioSource* audioSource = nullptr;
    QIODevice* audioIo = nullptr;
    QAudioSink* audioSinkDev = nullptr;
    QIODevice* playbackIo = nullptr;
    QTimer* playbackTimer = nullptr;

    MediaEngine* engine = nullptr;

    // ——— 资源清理（GUI 线程调用）———
    void reset()
    {
        running = false;

        std::shared_ptr<rtc::PeerConnection> oldPc;
        std::shared_ptr<rtc::Track> oldVideo;
        std::shared_ptr<rtc::Track> oldAudio;
        {
            std::lock_guard<std::mutex> lk(pcMutex);
            oldPc = std::move(pc);
            oldVideo = std::move(videoTrack);
            oldAudio = std::move(audioTrack);
        }
        if (oldPc) {
            oldPc->close();
        }
        oldVideo.reset();
        oldAudio.reset();
        oldPc.reset();
        {
            std::lock_guard<std::mutex> lk(pendingIceMutex);
            pendingIce.clear();
        }

        if (capSession) {
            delete capSession;
            capSession = nullptr;
        }
        if (camera) {
            camera->stop();
            delete camera;
            camera = nullptr;
        }
        if (audioSource) {
            audioSource->stop();
            delete audioSource;
            audioSource = nullptr;
        }
        audioIo = nullptr;
        if (audioSinkDev) {
            audioSinkDev->stop();
            delete audioSinkDev;
            audioSinkDev = nullptr;
        }
        playbackIo = nullptr;
        if (playbackTimer) {
            playbackTimer->stop();
            playbackTimer->deleteLater();
            playbackTimer = nullptr;
        }

        closeVideoEncoder();
        {
            std::lock_guard<std::mutex> lk(decMutex);
            closeVideoDecoderLocked();
        }
        {
            std::lock_guard<std::mutex> lk(decAudioMutex);
            if (opusDec) {
                opus_decoder_destroy(opusDec);
                opusDec = nullptr;
            }
        }
        if (opusEnc) {
            opus_encoder_destroy(opusEnc);
            opusEnc = nullptr;
        }
        if (swrCapture) {
            swr_free(&swrCapture);
        }
        if (swrPlayback) {
            swr_free(&swrPlayback);
        }
        srcFrameBytes = 0;
        srcSamplesPerFrame = 0;
        audioAccum.clear();
        samplesSent = 0;
        cameraWarmupSkips = 2;
        {
            QMutexLocker lk(&pcmOutMutex);
            pcmOut.clear();
        }
    }

    void closeVideoEncoder()
    {
        if (swsEnc) {
            sws_freeContext(swsEnc);
            swsEnc = nullptr;
        }
        if (venc) {
            avcodec_free_context(&venc);
            venc = nullptr;
        }
        encW = encH = 0;
        encPts = 0;
    }

    void closeVideoDecoderLocked()
    {
        if (swsDec) {
            sws_freeContext(swsDec);
            swsDec = nullptr;
        }
        if (vdec) {
            avcodec_free_context(&vdec);
            vdec = nullptr;
        }
        gotVideoKeyframe = false;
    }

    bool ensureVideoEncoder(int w, int h)
    {
        if (w % 2 != 0) {
            w -= 1;
        }
        if (h % 2 != 0) {
            h -= 1;
        }
        if (venc && w == encW && h == encH) {
            return true;
        }
        closeVideoEncoder();

        const AVCodec* codec = avcodec_find_encoder_by_name("libopenh264");
        if (!codec) {
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        }
        if (!codec) {
            LOG_ERROR("no h264 encoder available (libopenh264 missing)");
            return false;
        }

        venc = avcodec_alloc_context3(codec);
        venc->width = w;
        venc->height = h;
        venc->time_base = AVRational{1, 90000};
        venc->framerate = AVRational{kTargetFps, 1};
        venc->bit_rate = 800000;
        venc->gop_size = kTargetFps; // 每秒一个关键帧，中途入会最多等 1s
        venc->max_b_frames = 0;
        venc->pix_fmt = AV_PIX_FMT_YUV420P;
        if (avcodec_open2(venc, codec, nullptr) < 0) {
            LOG_ERROR("open h264 encoder failed");
            closeVideoEncoder();
            return false;
        }

        swsEnc = sws_getContext(w, h, AV_PIX_FMT_RGBA, w, h, AV_PIX_FMT_YUV420P,
                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        encW = w;
        encH = h;
        encPts = 0;
        return true;
    }

    // GUI 线程：摄像头帧 → H264 → RTP track
    void onCameraFrame(const QVideoFrame& frame)
    {
        if (!running.load()) {
            return;
        }
        // 摄像头启动初期帧内容可能未就绪，跳过防止把脏帧编码发出去
        if (cameraWarmupSkips > 0) {
            cameraWarmupSkips--;
            return;
        }
        if (!frameThrottle.isValid() || frameThrottle.elapsed() >= kMinFrameIntervalMs) {
            frameThrottle.restart();
            encodeFrame(frame);
        }
    }

    void encodeFrame(const QVideoFrame& vf)
    {
        if (!videoTrack || !videoTrack->isOpen()) {
            return;
        }
        QImage img = vf.toImage();
        if (img.isNull()) {
            // 只记一次，避免摄像头故障时逐帧刷屏
            static bool s_logged = false;
            if (!s_logged) {
                s_logged = true;
                LOG_ERROR("QVideoFrame::toImage() returned null, check camera frame format");
            }
            return;
        }
        img = img.convertToFormat(QImage::Format_RGBA8888);
        if (!ensureVideoEncoder(img.width(), img.height())) {
            return;
        }

        AVFrame* f = av_frame_alloc();
        f->format = AV_PIX_FMT_YUV420P;
        f->width = encW;
        f->height = encH;
        if (av_frame_get_buffer(f, 0) < 0) {
            av_frame_free(&f);
            return;
        }
        const uint8_t* srcSlice[1] = {img.bits()};
        int srcStride[1] = {static_cast<int>(img.bytesPerLine())};
        // 按编码器高度缩放（img 奇数尺寸时比 encH 多一行）
        sws_scale(swsEnc, srcSlice, srcStride, 0, encH, f->data, f->linesize);
        f->pts = encPts;
        encPts += 90000 / kTargetFps;

        if (avcodec_send_frame(venc, f) == 0) {
            AVPacket* pkt = av_packet_alloc();
            while (avcodec_receive_packet(venc, pkt) == 0) {
                rtc::binary data(reinterpret_cast<const std::byte*>(pkt->data),
                                 reinterpret_cast<const std::byte*>(pkt->data) + pkt->size);
                const auto ts = std::chrono::duration<double, std::micro>(
                    static_cast<double>(encPts) * 1000000.0 / 90000.0);
                try {
                    videoTrack->sendFrame(data, ts);
                } catch (const std::exception& e) {
                    LOG_ERROR(std::string("send video frame failed: ") + e.what());
                }
                av_packet_unref(pkt);
            }
            av_packet_free(&pkt);
        }
        av_frame_free(&f);
    }

    // rtc 线程：RTP 解包后的 H264 帧 → 解码 → 推给 GUI 渲染
    void onRemoteVideoFrame(const rtc::binary& frame)
    {
        if (!running.load()) {
            return;
        }
        QVideoFrame out = decodeFrame(frame);
        if (out.isValid()) {
            QMetaObject::invokeMethod(
                engine,
                [this, out]() {
                    if (running.load() && remoteSink) {
                        remoteSink->setVideoFrame(out);
                    }
                },
                Qt::QueuedConnection);
        }
    }

    QVideoFrame decodeFrame(const rtc::binary& frame)
    {
        std::lock_guard<std::mutex> lk(decMutex);
        if (!vdec) {
            const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
            if (!codec) {
                return QVideoFrame();
            }
            vdec = avcodec_alloc_context3(codec);
            vdec->pix_fmt = AV_PIX_FMT_YUV420P;
            if (avcodec_open2(vdec, codec, nullptr) < 0) {
                LOG_ERROR("open h264 decoder failed");
                closeVideoDecoderLocked();
                return QVideoFrame();
            }
        }

        AVPacket* pkt = av_packet_alloc();
        av_new_packet(pkt, static_cast<int>(frame.size()));
        memcpy(pkt->data, frame.data(), frame.size());
        QVideoFrame result;
        if (avcodec_send_packet(vdec, pkt) == 0) {
            AVFrame* f = av_frame_alloc();
            while (avcodec_receive_frame(vdec, f) == 0) {
                // 首个关键帧（含 SPS/PPS）解出来之前丢弃输出，避免接听瞬间花屏
                if (!gotVideoKeyframe) {
                    if (f->flags & AV_FRAME_FLAG_KEY) {
                        gotVideoKeyframe = true;
                    } else {
                        continue;
                    }
                }
                result = yuv420ToVideoFrame(f);
            }
            av_frame_free(&f);
        }
        av_packet_free(&pkt);
        return result;
    }

    QVideoFrame yuv420ToVideoFrame(const AVFrame* f)
    {
        const int w = f->width;
        const int h = f->height;
        if (w <= 0 || h <= 0) {
            return QVideoFrame();
        }
        swsDec = sws_getCachedContext(swsDec, w, h, static_cast<AVPixelFormat>(f->format),
                                      w, h, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR,
                                      nullptr, nullptr, nullptr);

        QImage img(w, h, QImage::Format_RGBA8888);
        uint8_t* dstSlice[1] = {img.bits()};
        int dstStride[1] = {static_cast<int>(img.bytesPerLine())};
        sws_scale(swsDec, f->data, f->linesize, 0, h, dstSlice, dstStride);

        QVideoFrameFormat vfmt(QSize(w, h), QVideoFrameFormat::Format_RGBA8888);
        QVideoFrame frame(vfmt);
        if (!frame.map(QVideoFrame::WriteOnly)) {
            return QVideoFrame();
        }
        const int rowBytes = w * 4;
        for (int y = 0; y < h; ++y) {
            memcpy(frame.bits(0) + y * frame.bytesPerLine(0), img.constScanLine(y), rowBytes);
        }
        frame.unmap();
        return frame;
    }

    // ——— 音频 ———
    static AVSampleFormat toAvSampleFormat(QAudioFormat::SampleFormat fmt)
    {
        switch (fmt) {
        case QAudioFormat::UInt8:
            return AV_SAMPLE_FMT_U8;
        case QAudioFormat::Int16:
            return AV_SAMPLE_FMT_S16;
        case QAudioFormat::Int32:
            return AV_SAMPLE_FMT_S32;
        case QAudioFormat::Float:
            return AV_SAMPLE_FMT_FLT;
        default:
            return AV_SAMPLE_FMT_S16;
        }
    }

    void onAudioCaptureReady()
    {
        if (!running.load() || !audioIo || srcFrameBytes <= 0) {
            return;
        }
        audioAccum.append(audioIo->readAll());
        while (audioAccum.size() >= srcFrameBytes) {
            encodeOneAudioFrame(audioAccum.constData());
            audioAccum.remove(0, srcFrameBytes);
        }
    }

    // pcm 已是 48k/mono/Int16（无重采样时即设备原始数据）
    void encodeOneAudioFrame(const char* pcm)
    {
        if (swrCapture) {
            uint8_t outData[kAudioFrameBytes];
            uint8_t* outBuf[1] = {outData};
            const uint8_t* in[1] = {reinterpret_cast<const uint8_t*>(pcm)};
            const int n = swr_convert(swrCapture, outBuf, kOpusFrameSamples, in,
                                      srcSamplesPerFrame);
            if (n > 0) {
                encodeAudioFrame(reinterpret_cast<const char*>(outData));
            }
        } else {
            encodeAudioFrame(pcm);
        }
    }

    void encodeAudioFrame(const char* pcm)
    {
        if (!audioTrack || !audioTrack->isOpen() || !opusEnc) {
            return;
        }
        unsigned char out[400];
        const int n = opus_encode(opusEnc, reinterpret_cast<const opus_int16*>(pcm),
                                  kOpusFrameSamples, out, sizeof(out));
        if (n <= 0) {
            return;
        }
        rtc::binary data(reinterpret_cast<const std::byte*>(out),
                         reinterpret_cast<const std::byte*>(out) + n);
        samplesSent += kOpusFrameSamples;
        const auto ts = std::chrono::duration<double, std::micro>(
            static_cast<double>(samplesSent) * 1000000.0 / kAudioSampleRate);
        try {
            audioTrack->sendFrame(data, ts);
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("send audio frame failed: ") + e.what());
        }
    }

    // rtc 线程：Opus 包 → PCM → 播放缓冲
    void onRemoteAudioPacket(const rtc::binary& packet)
    {
        if (!running.load()) {
            return;
        }
        std::lock_guard<std::mutex> lk(decAudioMutex);
        if (!opusDec) {
            int err = 0;
            opusDec = opus_decoder_create(kAudioSampleRate, kAudioChannels, &err);
            if (err != OPUS_OK || !opusDec) {
                return;
            }
        }
        opus_int16 pcm[5760]; // 最长 120ms
        const int n = opus_decode(opusDec, reinterpret_cast<const unsigned char*>(packet.data()),
                                  static_cast<int>(packet.size()), pcm, 5760, 0);
        if (n <= 0) {
            // 解码失败通常是持续性的，只记一次避免逐包刷屏
            static bool s_logged = false;
            if (!s_logged) {
                s_logged = true;
                LOG_ERROR("opus_decode failed, bytes=" + std::to_string(packet.size()));
            }
            return;
        }
        QMutexLocker locker(&pcmOutMutex);
        pcmOut.append(reinterpret_cast<const char*>(pcm), n * kAudioChannels * 2);
        // 积压过多说明播放跟不上，丢到最近边界避免越积越延迟
        if (pcmOut.size() > kMaxPcmBacklogBytes) {
            pcmOut.remove(0, pcmOut.size() - kAudioFrameBytes);
        }
    }

    void drainPcmForPlayback()
    {
        if (!playbackIo) {
            return;
        }
        QByteArray chunk;
        {
            QMutexLocker locker(&pcmOutMutex);
            if (pcmOut.isEmpty()) {
                return;
            }
            chunk = pcmOut;
            pcmOut.clear();
        }
        if (swrPlayback) {
            const int maxOut = av_rescale_rnd(swr_get_delay(swrPlayback, kAudioSampleRate) +
                                                  chunk.size() / 2,
                                              outFmt.sampleRate(), kAudioSampleRate,
                                              AV_ROUND_UP);
            QByteArray conv(maxOut * outFmt.bytesPerFrame(), Qt::Uninitialized);
            const uint8_t* in[1] = {reinterpret_cast<const uint8_t*>(chunk.constData())};
            uint8_t* out[1] = {reinterpret_cast<uint8_t*>(conv.data())};
            const int n = swr_convert(swrPlayback, out, maxOut, in, chunk.size() / 2);
            if (n > 0) {
                playbackIo->write(conv.constData(), n * outFmt.bytesPerFrame());
            }
        } else {
            playbackIo->write(chunk);
        }
    }
};

MediaEngine::MediaEngine() = default;
MediaEngine::~MediaEngine() = default;

void MediaEngine::init()
{
    if (_impl) {
        return;
    }
    _impl = std::make_unique<Impl>();
    _impl->engine = this;
    rtc::InitLogger(rtc::LogLevel::Warning);
}

void MediaEngine::attachVideoWidgets(QVideoWidget* local, QVideoWidget* remote)
{
    if (!_impl || _impl->voiceOnly) {
        return; // 语音通话无视频
    }
    QVideoSink* localSinkObj = local ? local->videoSink() : nullptr;
    if (localSinkObj) {
        _impl->localSink = localSinkObj;
        // 从窗口控件自带的 sink 取摄像头帧做编码（旧窗口销毁时连接自动断开）
        QObject::disconnect(localSinkObj, nullptr, this, nullptr);
        QObject::connect(localSinkObj, &QVideoSink::videoFrameChanged, this,
                         [this](const QVideoFrame& frame) {
                             if (_impl) {
                                 _impl->onCameraFrame(frame);
                             }
                         });
        if (_impl->capSession) {
            _impl->capSession->setVideoSink(localSinkObj);
        }
    }
    QVideoSink* remoteSinkObj = remote ? remote->videoSink() : nullptr;
    if (remoteSinkObj) {
        _impl->remoteSink = remoteSinkObj;
    }
}

void MediaEngine::start(bool asCaller, int peerUid, const QString& callId,
                        const QString& callType)
{
    if (!_impl) {
        init();
    }
    stop();
    Impl& im = *_impl;
    im.running = true;
    const bool voiceOnly = (callType == CallType::VOICE);
    im.voiceOnly = voiceOnly;

    // ——— PeerConnection ———
    rtc::Configuration cfg;
    cfg.iceServers.emplace_back("stun:stun.miwifi.com:3478");
    cfg.iceServers.emplace_back("stun:stun.l.google.com:19302");

    auto pc = std::make_shared<rtc::PeerConnection>(cfg);
    {
        std::lock_guard<std::mutex> lk(im.pcMutex);
        im.pc = pc;
    }

    // 身份校验：挂断后旧 pc 的在途回调不得串进新通话
    const std::weak_ptr<rtc::PeerConnection> weakPc = pc;

    pc->onLocalDescription([this, weakPc](rtc::Description desc) {
        if (!_impl || !_impl->isCurrentPc(weakPc)) {
            return;
        }
        const QString type = QString::fromStdString(desc.typeString());
        const QString sdp = QString::fromStdString(std::string(desc));
        emit sig_send_sig(type == QStringLiteral("answer") ? CallSig::ANSWER : CallSig::OFFER, sdp);
    });

    pc->onLocalCandidate([this, weakPc](rtc::Candidate cand) {
        if (!_impl || !_impl->isCurrentPc(weakPc)) {
            return;
        }
        emit sig_send_sig(CallSig::ICE,
                          QString::fromStdString(std::string(cand)));
    });

    pc->onStateChange([this, weakPc](rtc::PeerConnection::State state) {
        if (!_impl || !_impl->isCurrentPc(weakPc)) {
            return;
        }
        using S = rtc::PeerConnection::State;
        switch (state) {
        case S::Connected:
            emit sig_status(QStringLiteral("通话中"));
            break;
        case S::Disconnected:
            emit sig_status(QStringLiteral("连接中断"));
            break;
        case S::Failed:
            emit sig_status(QStringLiteral("打洞失败"));
            emit sig_media_failed();
            break;
        default:
            break;
        }
    });

    // ——— 本端收发 track（同一 m-line 双向：packetizer→depacketizer→rtcp 链）———
    // 语音通话只建音频轨：无视频 m-line、不采摄像头、对端也不会收到黑屏视频
    const uint32_t ssrcAudio = QRandomGenerator::global()->generate() & 0x7fffffffu;
    const std::string cnameA = "foolchat-audio";

    if (!voiceOnly) {
        const uint32_t ssrcVideo = QRandomGenerator::global()->generate() & 0x7fffffffu;
        const std::string cnameV = "foolchat-video";
        rtc::Description::Video video("video", rtc::Description::Direction::SendRecv);
        video.addH264Codec(kVideoPayloadType);
        video.addSSRC(ssrcVideo, cnameV, "call", cnameV);
        {
            std::lock_guard<std::mutex> lk(im.pcMutex);
            im.videoTrack = pc->addTrack(video);
        }

        auto vRtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
            ssrcVideo, cnameV, kVideoPayloadType, rtc::H264RtpPacketizer::ClockRate);
        auto vPacketizer = std::make_shared<rtc::H264RtpPacketizer>(
            rtc::NalUnit::Separator::StartSequence, vRtpConfig);
        vPacketizer->addToChain(std::make_shared<rtc::H264RtpDepacketizer>(
            rtc::NalUnit::Separator::StartSequence));
        vPacketizer->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
        im.videoTrack->setMediaHandler(vPacketizer);
        const std::weak_ptr<rtc::Track> weakVideoTrack = im.videoTrack;
        im.videoTrack->onFrame([this, weakVideoTrack](rtc::binary frame, rtc::FrameInfo info) {
            Q_UNUSED(info);
            if (_impl && _impl->isCurrentTrack(weakVideoTrack, true)) {
                _impl->onRemoteVideoFrame(frame);
            }
        });
    }

    rtc::Description::Audio audio("audio", rtc::Description::Direction::SendRecv);
    audio.addOpusCodec(kAudioPayloadType);
    audio.addSSRC(ssrcAudio, cnameA, "call", cnameA);
    {
        std::lock_guard<std::mutex> lk(im.pcMutex);
        im.audioTrack = pc->addTrack(audio);
    }

    auto aRtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
        ssrcAudio, cnameA, kAudioPayloadType, rtc::OpusRtpPacketizer::DefaultClockRate);
    auto aPacketizer = std::make_shared<rtc::OpusRtpPacketizer>(aRtpConfig);
    // OpusRtpDepacketizer 模板未被 datachannel.dll 导出，用等价基类：
    // 逐包剥 RTP 头并生成 FrameInfo（Opus 无分片，一包即一帧）
    aPacketizer->addToChain(std::make_shared<rtc::RtpDepacketizer>(
        rtc::OpusRtpPacketizer::DefaultClockRate));
    aPacketizer->addToChain(std::make_shared<rtc::RtcpReceivingSession>());
    im.audioTrack->setMediaHandler(aPacketizer);
    const std::weak_ptr<rtc::Track> weakAudioTrack = im.audioTrack;
    im.audioTrack->onFrame([this, weakAudioTrack](rtc::binary frame, rtc::FrameInfo info) {
        Q_UNUSED(info);
        if (_impl && _impl->isCurrentTrack(weakAudioTrack, false)) {
            _impl->onRemoteAudioPacket(frame);
        }
    });

    // ——— 采集侧初始化 ———
    im.frameThrottle.invalidate();
    if (!voiceOnly) {
        setupCamera();
    }
    setupAudio();

    // ——— 主叫发起 offer；被叫等远端 offer 触发自动 answer ———
    if (asCaller) {
        try {
            pc->setLocalDescription();
        } catch (const std::exception& e) {
            LOG_ERROR(std::string("setLocalDescription failed: ") + e.what());
            emit sig_media_failed();
        }
    }
    Q_UNUSED(peerUid);
    Q_UNUSED(callId);
}

void MediaEngine::setupCamera()
{
    // 成员方法声明在 Impl 内不方便访问 engine，这里直接操作 _impl
    Impl& im = *_impl;
    const auto cams = QMediaDevices::videoInputs();
    if (cams.isEmpty()) {
        LOG_ERROR("no camera found, video send disabled");
        return;
    }
    const auto device = cams.first();
    im.camera = new QCamera(device);
    im.cameraFormatRetried = false;
    // 常见两种失败：自定义格式驱动不支持（回退默认格式重试一次）、
    // 摄像头被另一进程独占（同机双开测试时的预期，无法恢复）
    QObject::connect(im.camera, &QCamera::errorOccurred, this,
                     [this](QCamera::Error err, const QString& errStr) {
                         LOG_ERROR("camera error code=" + std::to_string(int(err)) +
                                   " (" + errStr.toStdString() + ")");
                         emit sig_status(QStringLiteral("摄像头不可用: %1").arg(errStr));
                         if (_impl && _impl->camera && !_impl->cameraFormatRetried &&
                             !_impl->camera->cameraFormat().isNull()) {
                             _impl->cameraFormatRetried = true;
                             _impl->camera->setCameraFormat(QCameraFormat());
                             _impl->camera->start();
                         }
                     });

    // 选最接近 640x480 且 >=15fps 的采集格式
    QCameraFormat best;
    bool found = false;
    int bestScore = std::numeric_limits<int>::max();
    for (const auto& fmt : device.videoFormats()) {
        int score = std::abs(fmt.resolution().width() - 640) +
                    std::abs(fmt.resolution().height() - 480);
        if (fmt.maxFrameRate() >= kTargetFps) {
            score -= 100;
        }
        if (score < bestScore) {
            bestScore = score;
            best = fmt;
            found = true;
        }
    }
    if (found) {
        im.camera->setCameraFormat(best);
    }

    im.capSession = new QMediaCaptureSession();
    im.capSession->setCamera(im.camera);
    // attachVideoWidgets 先于 start 到达时在此挂上，否则等 attach 时再挂
    if (im.localSink) {
        im.capSession->setVideoSink(im.localSink.data());
    }
    im.camera->start();
}

void MediaEngine::setupAudio()
{
    Impl& im = *_impl;

    const auto inDev = QMediaDevices::defaultAudioInput();
    if (inDev.isNull()) {
        LOG_ERROR("no audio input found, audio send disabled");
        return;
    }
    const auto outDev = QMediaDevices::defaultAudioOutput();

    // 优先用 48k/mono/Int16（与 Opus 一致，无需重采样）；
    // 输入/输出端点各自独立协商——之前把输入设备传给 QAudioSink 导致
    // WASAPI 渲染客户端创建失败（0x88890003）
    QAudioFormat target;
    target.setSampleRate(kAudioSampleRate);
    target.setChannelCount(kAudioChannels);
    target.setSampleFormat(QAudioFormat::Int16);
    im.inFmt = inDev.isFormatSupported(target) ? target : inDev.preferredFormat();
    im.outFmt = outDev.isNull() ? target
                                : (outDev.isFormatSupported(target) ? target
                                                                    : outDev.preferredFormat());

    const auto isTarget = [](const QAudioFormat& f) {
        return f.channelCount() == kAudioChannels && f.sampleRate() == kAudioSampleRate &&
               f.sampleFormat() == QAudioFormat::Int16;
    };
    const bool captureNeedSwr = !isTarget(im.inFmt);
    const bool playbackNeedSwr = !isTarget(im.outFmt);

    auto makeSwr = [](const AVChannelLayout& inLayout, AVSampleFormat inFmt, int inRate,
                      const AVChannelLayout& outLayout, AVSampleFormat outFmt, int outRate,
                      SwrContext** out) -> bool {
        const int ret = swr_alloc_set_opts2(out, &outLayout, outFmt, outRate, &inLayout, inFmt,
                                            inRate, 0, nullptr);
        if (ret < 0) {
            return false;
        }
        if (swr_init(*out) < 0) {
            swr_free(out);
            return false;
        }
        return true;
    };

    if (captureNeedSwr) {
        AVChannelLayout inLayout;
        AVChannelLayout outLayout;
        av_channel_layout_default(&inLayout, im.inFmt.channelCount());
        av_channel_layout_default(&outLayout, kAudioChannels);
        if (!makeSwr(inLayout, Impl::toAvSampleFormat(im.inFmt.sampleFormat()),
                     im.inFmt.sampleRate(), outLayout, AV_SAMPLE_FMT_S16, kAudioSampleRate,
                     &im.swrCapture)) {
            LOG_ERROR("init capture resampler failed, audio send disabled");
            return;
        }
    }
    if (playbackNeedSwr) {
        AVChannelLayout inLayout;
        AVChannelLayout outLayout;
        av_channel_layout_default(&inLayout, kAudioChannels);
        av_channel_layout_default(&outLayout, im.outFmt.channelCount());
        if (!makeSwr(inLayout, AV_SAMPLE_FMT_S16, kAudioSampleRate, outLayout,
                     Impl::toAvSampleFormat(im.outFmt.sampleFormat()), im.outFmt.sampleRate(),
                     &im.swrPlayback)) {
            LOG_ERROR("init playback resampler failed, audio playback disabled");
            swr_free(&im.swrCapture);
            return;
        }
    }

    im.srcFrameBytes = im.inFmt.bytesForDuration(20000);
    im.srcSamplesPerFrame = im.inFmt.bytesPerFrame() > 0
                                ? im.srcFrameBytes / im.inFmt.bytesPerFrame()
                                : kOpusFrameSamples;
    if (im.srcFrameBytes <= 0 || im.srcSamplesPerFrame <= 0) {
        LOG_ERROR("invalid audio frame size");
        swr_free(&im.swrCapture);
        swr_free(&im.swrPlayback);
        return;
    }

    int err = 0;
    im.opusEnc = opus_encoder_create(kAudioSampleRate, kAudioChannels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !im.opusEnc) {
        im.opusEnc = nullptr;
        swr_free(&im.swrCapture);
        swr_free(&im.swrPlayback);
        return;
    }
    opus_encoder_ctl(im.opusEnc, OPUS_SET_BITRATE(32000));
    opus_encoder_ctl(im.opusEnc, OPUS_SET_COMPLEXITY(5));

    im.audioSource = new QAudioSource(inDev, im.inFmt, this);
    im.audioIo = im.audioSource->start();
    if (im.audioIo) {
        QObject::connect(im.audioIo, &QIODevice::readyRead, this, [this]() {
            if (_impl) {
                _impl->onAudioCaptureReady();
            }
        });
    }

    // 播放：定时把解码 PCM（必要时重采样）灌给 QAudioSink（输出端点）
    im.audioSinkDev = new QAudioSink(outDev, im.outFmt, this);
    im.playbackIo = im.audioSinkDev->start();
    if (!im.playbackIo) {
        LOG_ERROR("audio playback start failed, no sound on this side");
    }
    im.playbackTimer = new QTimer(this);
    im.playbackTimer->setInterval(20);
    QObject::connect(im.playbackTimer, &QTimer::timeout, this, [this]() {
        if (_impl) {
            _impl->drainPcmForPlayback();
        }
    });
    im.playbackTimer->start();
}

void MediaEngine::stop()
{
    if (!_impl) {
        return;
    }
    _impl->reset();
}

void MediaEngine::onRemoteOffer(const QString& sdp)
{
    if (!_impl || !_impl->running.load() || !_impl->pc) {
        return;
    }
    try {
        _impl->pc->setRemoteDescription(rtc::Description(sdp.toStdString(), "offer"));
        flushPendingIce();
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("setRemoteDescription offer failed: ") + e.what());
        emit sig_media_failed();
    }
}

void MediaEngine::onRemoteAnswer(const QString& sdp)
{
    if (!_impl || !_impl->running.load() || !_impl->pc) {
        return;
    }
    try {
        _impl->pc->setRemoteDescription(rtc::Description(sdp.toStdString(), "answer"));
        flushPendingIce();
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("setRemoteDescription answer failed: ") + e.what());
        emit sig_media_failed();
    }
}

void MediaEngine::onRemoteIce(const QString& candidate)
{
    if (!_impl || !_impl->running.load() || !_impl->pc) {
        return;
    }
    try {
        _impl->pc->addRemoteCandidate(rtc::Candidate(candidate.toStdString()));
    } catch (const std::exception&) {
        // 远端 description 未就绪，先缓存
        std::lock_guard<std::mutex> lk(_impl->pendingIceMutex);
        _impl->pendingIce.push_back(candidate.toStdString());
    }
}

void MediaEngine::flushPendingIce()
{
    std::vector<std::string> pending;
    {
        std::lock_guard<std::mutex> lk(_impl->pendingIceMutex);
        pending.swap(_impl->pendingIce);
    }
    for (const auto& c : pending) {
        try {
            _impl->pc->addRemoteCandidate(rtc::Candidate(c));
        } catch (const std::exception&) {
        }
    }
}
