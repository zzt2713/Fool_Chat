#include "callwindow.h"
#include "ElaText.h"
#include "callmgr.h"
#include "mediaengine.h"
#include "pixmaputil.h"
#include <QCloseEvent>
#include <QIcon>
#include <QLabel>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace {

constexpr int kLocalViewW = 160;
constexpr int kLocalViewH = 120;

QLabel* makeAvatar(const QString& path, int diameter, QWidget* parent)
{
    auto* lb = new QLabel(parent);
    lb->setFixedSize(diameter, diameter);
    lb->setAlignment(Qt::AlignCenter);
    if (!path.isEmpty()) {
        const QPixmap pm(path);
        if (!pm.isNull()) {
            lb->setPixmap(PixmapUtil::round(pm, QSize(diameter, diameter),
                                            parent->devicePixelRatioF()));
        }
    }
    lb->setStyleSheet("background: #333333; border-radius: "
                      + QString::number(diameter / 2) + "px;");
    return lb;
}

} // namespace

CallWindow::CallWindow(const QString& peerName, const QString& avatarPath, QWidget* parent)
    : ElaDialog(parent)
{
    _voiceMode = (CallManager::GetInstance()->callType() == CallType::VOICE);
    setWindowTitle(_voiceMode ? QStringLiteral("语音通话") : QStringLiteral("视频通话"));
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setModal(false);
    // 语音通话没有视频区，用紧凑布局（大头像 + 状态 + 挂断）
    resize(_voiceMode ? 380 : 880, _voiceMode ? 470 : 620);

    // 微信风格深色底（ElaDialog 无 setCentralWidget，布局直接挂对话框）
    setObjectName("CallWindow");
    setStyleSheet("#CallWindow { background: #191919; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 18);
    root->setSpacing(10);

    // ——— 顶部：小头像 + 名字 + 状态 ———
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(10);
    _smallAvatar = makeAvatar(avatarPath, 40, this);
    _nameLb = new ElaText(peerName, this);
    _nameLb->setTextPixelSize(18);
    _nameLb->setTextStyle(ElaTextType::Body);
    _statusLb = new ElaText(QStringLiteral("正在呼叫..."), this);
    _statusLb->setTextStyle(ElaTextType::Body);
    _statusLb->setStyleSheet("color: #8a8a8a;");
    topRow->addSpacing(4);
    topRow->addWidget(_smallAvatar);
    topRow->addWidget(_nameLb);
    topRow->addStretch();
    topRow->addWidget(_statusLb);
    topRow->addSpacing(4);
    root->addLayout(topRow);

    // ——— 中部：视频区（子控件手动定位：远端铺满 / 本地右下角 / 呼叫层居中）———
    _videoHost = new QWidget(this);
    _videoHost->setMinimumHeight(_voiceMode ? 260 : 360);
    _videoHost->setAutoFillBackground(false);

    _remoteView = new QVideoWidget(_videoHost);
    _remoteView->setObjectName("remoteView");
    _remoteView->hide(); // 呼叫中不显示，接通后 setConnected 打开

    _localView = new QVideoWidget(_videoHost);
    _localView->setObjectName("localView");
    _localView->setFixedSize(kLocalViewW, kLocalViewH);
    _localView->hide();
    _localView->setStyleSheet(
        "QVideoWidget#localView { border: 1px solid #333333; border-radius: 8px; }");

    // 呼叫中的居中大头像层
    _ringBox = new QWidget(_videoHost);
    auto* ringLay = new QVBoxLayout(_ringBox);
    ringLay->setContentsMargins(0, 0, 0, 0);
    ringLay->setSpacing(14);
    ringLay->addStretch();
    _bigAvatar = makeAvatar(avatarPath, 120, _ringBox);
    ringLay->addWidget(_bigAvatar, 0, Qt::AlignHCenter);
    auto* ringName = new ElaText(peerName, _ringBox);
    ringName->setTextPixelSize(22);
    ringName->setTextStyle(ElaTextType::Title);
    ringLay->addWidget(ringName, 0, Qt::AlignHCenter);
    ringLay->addStretch();

    root->addWidget(_videoHost, 1);

    // ——— 底部：红色挂断按钮 ———
    auto* btnRow = new QHBoxLayout();
    // png 即完整的红色圆形挂断按钮，透明显示
    auto* hangupBtn = new QToolButton(this);
    hangupBtn->setFixedSize(64, 64);
    hangupBtn->setIcon(QIcon(":/icons/no.png"));
    hangupBtn->setIconSize(QSize(64, 64));
    hangupBtn->setToolTip(QStringLiteral("挂断"));
    hangupBtn->setCursor(Qt::PointingHandCursor);
    hangupBtn->setStyleSheet(
        "QToolButton { background: transparent; border: none; }"
        "QToolButton:hover { background: transparent; }");
    connect(hangupBtn, &QToolButton::clicked, this, &CallWindow::close);
    btnRow->addStretch();
    btnRow->addWidget(hangupBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    auto media = MediaEngine::GetInstance();
    media->attachVideoWidgets(_localView, _remoteView);
    connect(media.get(), &MediaEngine::sig_status, this,
            [this](const QString& text) { _statusLb->setText(text); });
}

void CallWindow::setConnected()
{
    if (_voiceMode) {
        // 语音通话保持居中大头像，仅切状态
        _statusLb->setText(QStringLiteral("正在接通..."));
        relayout();
        return;
    }
    _ringBox->hide();
    _remoteView->show();
    _localView->show();
    _statusLb->setText(QStringLiteral("正在接通..."));
    relayout();
}

void CallWindow::closeEvent(QCloseEvent* event)
{
    auto mgr = CallManager::GetInstance();
    if (mgr && mgr->state() != CallState::Idle) {
        mgr->hangupCall();
    }
    ElaDialog::closeEvent(event);
}

void CallWindow::showEvent(QShowEvent* event)
{
    ElaDialog::showEvent(event);
    raise();
    activateWindow();
    relayout();
}

void CallWindow::resizeEvent(QResizeEvent* event)
{
    ElaDialog::resizeEvent(event);
    relayout();
}

void CallWindow::relayout()
{
    if (!_videoHost) {
        return;
    }
    const QSize hostSize = _videoHost->size();
    if (_remoteView) {
        _remoteView->setGeometry(0, 0, hostSize.width(), hostSize.height());
    }
    if (_localView && _localView->isVisible()) {
        _localView->move(hostSize.width() - kLocalViewW - 14,
                         hostSize.height() - kLocalViewH - 14);
    }
    if (_ringBox && _ringBox->isVisible()) {
        const QSize ringSize = _ringBox->sizeHint().expandedTo(QSize(240, 240));
        _ringBox->resize(ringSize);
        _ringBox->move((hostSize.width() - ringSize.width()) / 2,
                       (hostSize.height() - ringSize.height()) / 2);
    }
}
