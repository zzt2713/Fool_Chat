#ifndef CALLUI_H
#define CALLUI_H

#include "ElaDialog.h"
#include "ElaText.h"
#include "callmgr.h"
#include "callwindow.h"
#include "pixmaputil.h"
#include "usermgr.h"
#include "msgtip.h"
#include <QIcon>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPointer>
#include <memory>

/**
 * @brief 统一的发起通话入口（视频/语音按钮共用）
 * 呼叫中再点=取消；通话中=提示忙线
 */
inline void TryStartCall(QWidget* host, int peerUid, const QString& type)
{
    if (peerUid <= 0) {
        return;
    }
    auto mgr = CallManager::GetInstance();
    if (mgr->state() == CallState::Outgoing) {
        mgr->hangupCall();
        ADDMSG(ElaMessageBarType::Top, "已取消呼叫", host, 2, 1500);
        return;
    }
    if (mgr->state() != CallState::Idle) {
        ADDMSG(ElaMessageBarType::Top, "当前已有通话进行中", host, 2, 2000);
        return;
    }
    mgr->startCall(peerUid, type);
}

/**
 * @brief 通话 UI 接线：
 * - 主叫点击呼叫即打开通话窗口（呼叫中大头像 → 接通切视频）
 * - 被叫来电弹窗（头像 + 圆形 接听/拒接 图标按钮，微信式）
 * - 各结束路径统一关窗
 * 在主窗口构造时调用一次。
 */
inline void InitCallUi(QWidget* host)
{
    if (host == nullptr) {
        return;
    }
    auto mgr = CallManager::GetInstance();
    mgr->init();

    // 跨 lambda 共享：当前打开的来电框 / 通话窗口（QPointer 随销毁自动置空）
    auto activeDlg = std::make_shared<QPointer<ElaDialog>>();
    auto callWnd = std::make_shared<QPointer<CallWindow>>();
    // 打开通话窗口（旧的还挂着就先关，绝不静默跳过）
    auto openCallWindow = [host, activeDlg, callWnd](bool connected) {
        // 接听路径：通话窗弹出前先关来电弹窗，避免窗口压住来电大头像
        if (auto* ringing = activeDlg->data()) {
            ringing->close();
        }
        const int peer = CallManager::GetInstance()->peerUid();
        auto fi = UserMgr::GetInstance()->GetFriendById(peer);
        const QString name = fi ? fi->DisplayName() : QString::number(peer);
        const QString avatar = fi ? fi->_icon : QString();
        if (auto* stale = callWnd->data()) {
            stale->close();
        }
        auto* w = new CallWindow(name, avatar, host);
        *callWnd = w;
        w->setAttribute(Qt::WA_DeleteOnClose);
        if (connected) {
            w->setConnected();
        }
        w->moveToCenter();
        w->show();
        w->raise();
    };

    // 呼叫中：主叫立刻拿到微信式呼叫窗口
    QObject::connect(mgr.get(), &CallManager::sig_call_outgoing, host,
                     [openCallWindow](int peerUid) {
                         Q_UNUSED(peerUid);
                         openCallWindow(false);
                     });

    // 接通：被叫开窗；主叫已有窗口则切到视频态
    QObject::connect(mgr.get(), &CallManager::sig_call_connected, host,
                     [callWnd, openCallWindow](bool asCaller) {
                         Q_UNUSED(asCaller);
                         if (auto* w = callWnd->data()) {
                             w->setConnected();
                         } else {
                             openCallWindow(true);
                         }
                     });

    auto closeCallWindow = [callWnd]() {
        if (auto* w = callWnd->data()) {
            w->close();
        }
    };
    QObject::connect(mgr.get(), &CallManager::sig_call_ended, host, [closeCallWindow](int) {
        closeCallWindow();
    });
    QObject::connect(mgr.get(), &CallManager::sig_call_offline, host, [closeCallWindow]() {
        closeCallWindow();
    });
    QObject::connect(mgr.get(), &CallManager::sig_call_rejected, host, [closeCallWindow]() {
        closeCallWindow();
    });

    // ——— 被叫来电弹窗：头像 + 圆形接听/拒接（yes.png / no.png）———
    QObject::connect(mgr.get(), &CallManager::sig_incoming_call, host,
                     [host, mgr, activeDlg](int fromuid, QString callId) {
                         // 上一通的来电框若还挂着（deleteLater 未处理），先关掉
                         if (auto* old = activeDlg->data()) {
                             old->close();
                         }
                         auto fi = UserMgr::GetInstance()->GetFriendById(fromuid);
                         const QString name = fi ? fi->DisplayName() : QString::number(fromuid);
                         const QString avatar = fi ? fi->_icon : QString();

                         auto* dlg = new ElaDialog(host);
                         *activeDlg = dlg;
                         dlg->setAttribute(Qt::WA_DeleteOnClose);
                         dlg->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
                         dlg->setObjectName("IncomingCallDlg");
                         dlg->setStyleSheet("#IncomingCallDlg { background: #191919; }");
                         dlg->resize(380, 470);

                         auto* root = new QVBoxLayout(dlg);
                         root->setContentsMargins(28, 34, 28, 26);
                         root->setSpacing(14);
                         root->addStretch();

                         auto* avatarLb = new QLabel(dlg);
                         avatarLb->setFixedSize(96, 96);
                         avatarLb->setAlignment(Qt::AlignCenter);
                         if (!avatar.isEmpty()) {
                             const QPixmap pm(avatar);
                             if (!pm.isNull()) {
                                 avatarLb->setPixmap(PixmapUtil::round(pm, QSize(96, 96),
                                                                       dlg->devicePixelRatioF()));
                             }
                         }
                         avatarLb->setStyleSheet("background: #333333; border-radius: 48px;");
                         root->addWidget(avatarLb, 0, Qt::AlignHCenter);

                         auto* titleLabel = new ElaText(name, dlg);
                         titleLabel->setTextPixelSize(28);
                         titleLabel->setTextStyle(ElaTextType::Title);
                         root->addWidget(titleLabel, 0, Qt::AlignHCenter);

                         const bool voiceCall = (mgr->callType() == CallType::VOICE);
                         auto* subTitle = new ElaText(
                             voiceCall ? QStringLiteral("邀请你进行语音通话")
                                       : QStringLiteral("邀请你进行视频通话"),
                             dlg);
                         subTitle->setTextStyle(ElaTextType::Body);
                         root->addWidget(subTitle, 0, Qt::AlignHCenter);

                         root->addStretch();

                         auto makeCircleBtn = [dlg](const QString& iconPath) {
                             auto* b = new QToolButton(dlg);
                             b->setFixedSize(64, 64);
                             b->setIcon(QIcon(iconPath));
                             b->setIconSize(QSize(64, 64));
                             b->setCursor(Qt::PointingHandCursor);
                             b->setStyleSheet(
                                 "QToolButton { background: transparent; border: none; }"
                                 "QToolButton:hover { background: transparent; }"
                                 "QToolButton:pressed { background: transparent; }");
                             return b;
                         };
                         auto makeBtnCol = [&](QToolButton* btn, const QString& label) {
                             auto* col = new QWidget(dlg);
                             auto* lay = new QVBoxLayout(col);
                             lay->setContentsMargins(0, 0, 0, 0);
                             lay->setSpacing(8);
                             lay->addWidget(btn, 0, Qt::AlignHCenter);
                             auto* lb = new ElaText(label, col);
                             lb->setTextStyle(ElaTextType::Body);
                             lay->addWidget(lb, 0, Qt::AlignHCenter);
                             return col;
                         };

                         auto* btnRow = new QHBoxLayout();
                         btnRow->setSpacing(56);
                         btnRow->addStretch();
                         auto* rejectBtn = makeCircleBtn(":/icons/no.png");
                         btnRow->addWidget(makeBtnCol(rejectBtn, QStringLiteral("拒绝")));
                         auto* acceptBtn = makeCircleBtn(":/icons/yes.png");
                         btnRow->addWidget(makeBtnCol(acceptBtn, QStringLiteral("接听")));
                         btnRow->addStretch();
                         root->addLayout(btnRow);

                         QObject::connect(rejectBtn, &QToolButton::clicked, dlg, [mgr, dlg]() {
                             mgr->rejectCall();
                             dlg->close();
                         });
                         QObject::connect(acceptBtn, &QToolButton::clicked, dlg, [mgr, dlg]() {
                             // 先隐藏再接听：acceptCall 会同步弹通话窗，不能让它盖住本弹窗
                             dlg->hide();
                             mgr->acceptCall();
                             dlg->close();
                         });
                         // X 关闭且未选择时按拒接；带 callId 防止旧弹窗误拒新来电
                         QObject::connect(dlg, &QDialog::rejected, dlg, [mgr, callId]() {
                             if (mgr->state() == CallState::Incoming && mgr->callId() == callId) {
                                 mgr->rejectCall();
                             }
                         });
                         // 非模态：exec 嵌套循环会让挂断+快速重拨的信令交错
                         dlg->moveToCenter();
                         dlg->show();
                     });

    QObject::connect(mgr.get(), &CallManager::sig_call_offline, host, [host]() {
        ADDMSG(ElaMessageBarType::Top, "对方不在线，无法通话", host, 0, 2000);
    });

    QObject::connect(mgr.get(), &CallManager::sig_call_rejected, host, [host]() {
        ADDMSG(ElaMessageBarType::Top, "对方拒绝了通话", host, 3, 2000);
    });

    QObject::connect(mgr.get(), &CallManager::sig_call_ended, host,
                     [host, activeDlg](int reason) {
                         if (auto* dlg = activeDlg->data()) {
                             dlg->close();
                         }
                         if (reason == static_cast<int>(CallEndReason::Timeout)) {
                             ADDMSG(ElaMessageBarType::Top, "无人接听，通话结束", host, 1, 2000);
                         } else if (reason == static_cast<int>(CallEndReason::PeerHangup)) {
                             ADDMSG(ElaMessageBarType::Top, "对方已挂断", host, 2, 2000);
                         } else if (reason == static_cast<int>(CallEndReason::Disconnected)) {
                             ADDMSG(ElaMessageBarType::Top, "网络断开，通话结束", host, 0, 2000);
                         }
                     });
}

#endif // CALLUI_H
