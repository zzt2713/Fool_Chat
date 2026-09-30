#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include "ElaDialog.h"

class ElaText;
class QLabel;
class QVideoWidget;

/**
 * @brief 视频通话窗口（微信风格）
 * 呼叫中：深色背景 + 居中大头像 + 名字/状态；
 * 接通后：远端画面铺满 + 本地预览右下角小窗 + 底部红色挂断。
 * 关闭窗口 = 挂断通话。
 */
class CallWindow : public ElaDialog
{
    Q_OBJECT
public:
    CallWindow(const QString& peerName, const QString& avatarPath, QWidget* parent = nullptr);

    // 接通后切换到视频画面（隐藏呼叫中的头像层）
    void setConnected();

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void relayout();

    bool _voiceMode{false}; // 语音通话：无视频区，保持大头像布局
    QWidget* _videoHost{nullptr};
    QWidget* _ringBox{nullptr};   // 呼叫中的大头像层
    QLabel* _bigAvatar{nullptr};
    QLabel* _smallAvatar{nullptr};
    ElaText* _nameLb{nullptr};
    ElaText* _statusLb{nullptr};
    QVideoWidget* _localView{nullptr};
    QVideoWidget* _remoteView{nullptr};
};

#endif // CALLWINDOW_H
