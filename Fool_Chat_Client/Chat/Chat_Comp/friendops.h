#ifndef FRIENDOPS_H
#define FRIENDOPS_H

#include "ElaMenu.h"
#include "ElaContentDialog.h"
#include "ElaInputDialog.h"
#include "ElaText.h"
#include "usermgr.h"
#include "tcpmgr.h"
#include "global.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <functional>

/**
 * @brief 通用二次确认框（与项目删除好友/关闭窗口同款 ElaContentDialog 样式）
 * 确认后回调 onOk 执行实际动作
 */
inline void ConfirmAction(QWidget* host, const QString& title, const QString& desc,
                          const QString& okText, const std::function<void()>& onOk)
{
    if (host == nullptr) {
        return;
    }
    auto* dlg = new ElaContentDialog(host->window());
    dlg->resize(500, 250);
    dlg->setStyleSheet(
        "ElaContentDialog {"
        "   background: transparent;"
        "   border-radius: 12px;"
        "}"
    );
    dlg->setLeftButtonText("取消");
    dlg->setMiddleButtonVisible(false);
    dlg->setRightButtonText(okText);

    QWidget* customWidget = new QWidget(dlg);
    QVBoxLayout* layout = new QVBoxLayout(customWidget);
    layout->setContentsMargins(20, 30, 20, 10);

    auto* titleLabel = new ElaText(title, dlg);
    titleLabel->setTextPixelSize(36);
    titleLabel->setTextStyle(ElaTextType::Title);

    auto* subTitle = new ElaText(desc, dlg);
    subTitle->setTextStyle(ElaTextType::Body);

    layout->addWidget(titleLabel);
    layout->addSpacing(15);
    layout->addWidget(subTitle);
    layout->addStretch();
    dlg->setCentralWidget(customWidget);

    QObject::connect(dlg, &ElaContentDialog::leftButtonClicked, dlg, &QWidget::close);
    QObject::connect(dlg, &ElaContentDialog::rightButtonClicked, dlg, [dlg, onOk]() {
        onOk();
        dlg->close();
    });
    dlg->exec();
}

/**
 * @brief 二次确认后发送删除好友请求（AI 的 uid=-1 与本人都直接忽略）
 */
inline void AskDeleteFriend(QWidget* host, int peerUid)
{
    if (host == nullptr || peerUid == -1) {
        return;
    }
    const int selfUid = UserMgr::GetInstance()->GetUid();
    if (peerUid == selfUid) {
        return;
    }

    ConfirmAction(host, "删除好友", "删除后将不再与该好友互为好友，确定要删除吗", "删除",
                  [selfUid, peerUid]() {
                      QJsonObject obj;
                      obj["uid"] = selfUid;
                      obj["touid"] = peerUid;
                      QJsonDocument doc(obj);
                      emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_DELETE_FRIEND_REQ,
                                                                doc.toJson(QJsonDocument::Compact));
                  });
}

/**
 * @brief 条目右键菜单：删除好友（AI 条目在调用前已由 uid 守卫拦下）
 */
inline void PopFriendContextMenu(QWidget* host, const QPoint& pos, int peerUid)
{
    if (host == nullptr || peerUid == -1 || peerUid == UserMgr::GetInstance()->GetUid()) {
        return;
    }
    ElaMenu menu(host);
    QAction* backAction = menu.addAction(QStringLiteral("修改备注"));
    QObject::connect(backAction, &QAction::triggered, host, [host, peerUid]() {
        auto fi = UserMgr::GetInstance()->GetFriendById(peerUid);
        const QString cur = fi ? fi->_back : QString();
        bool ok = false;
        QString back = ElaInputDialog::getText(host, "修改备注", "仅自己可见，留空则显示昵称",
                                               "备注名", cur, &ok, "保存", "取消", 180, 260);
        if (!ok) {
            return;
        }
        QJsonObject obj;
        obj["uid"] = UserMgr::GetInstance()->GetUid();
        obj["touid"] = peerUid;
        obj["back"] = back.trimmed();
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_UPDATE_BACK_REQ,
                                                 QJsonDocument(obj).toJson(QJsonDocument::Compact));
    });
    menu.addSeparator();
    QAction* delAction = menu.addAction(QStringLiteral("删除好友"));
    QObject::connect(delAction, &QAction::triggered, host, [host, peerUid]() {
        AskDeleteFriend(host, peerUid);
    });
    menu.exec(host->mapToGlobal(pos));
}

#endif // FRIENDOPS_H
