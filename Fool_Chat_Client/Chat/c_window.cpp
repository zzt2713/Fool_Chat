#include "c_window.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMetaType>
#include <QLineEdit>
#include <memory>

#include "ElaDef.h"
#include "ElaContentDialog.h"
#include "ElaText.h"
#include "ElaMenu.h"
#include "Setting_Page/editprofiledlg.h"
#include "Chat_Comp/chattext.h"
#include "Chat_Comp/callui.h"
#include "ElaStatusBar.h"
#include "About_Page/c_about.h"
#include "Admin_Page/adminwid.h"
#include "../src/core/global.h"
#include "../src/widgets/msgtip.h"
#include "../src/widgets/notifypopup.h"
#include "../src/core/usermgr.h"
#include "../src/core/tcpmgr.h"
#include "../src/core/aimgr.h"
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFont>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>
#include <QSettings>

C_Window::C_Window(QWidget *parent):ElaWindow(parent),_contactMsg(0)
{
    initWindow();
    initContent();
    initClose();
    initGropAnnouncement();
    initGropMember();
    initNav();
    initStatus();
    initToolBar();
    initTray();
    InitCallUi(this);
    ADDMSG(ElaMessageBarType::Top,"聊天界面初始化成功",this,1,3000);

    // 好友申请消息连接
    qRegisterMetaType<std::shared_ptr<AddFriendApply>>("std::shared_ptr<AddFriendApply>");
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_friend_apply,this,&C_Window::slot_apply_friend);

    // 同意申请后角标重算（ApplyFriendPage 槽先执行更新状态，这里后算）
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_auth_rsp, this,
            [this](std::shared_ptr<AuthRsp>) { refreshApplyBadge(); });

    // 好友申请结果提示
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_apply_result, this, [this](int error) {
        if (error == 0) {
            ADDMSG(ElaMessageBarType::Top, "申请已发送，等待对方验证", this, 1, 2500);
        }
        else if (error == SERVER_ERR_ADD_REFUSED) {
            ADDMSG(ElaMessageBarType::Top, "对方设置了不允许添加好友", this, 0, 3000);
        }
        else {
            ADDMSG(ElaMessageBarType::Top, "申请发送失败", this, 0, 3000);
        }
    });

    // 备注修改成功提示（列表刷新由各列表自己接信号）
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_back_updated, this, [this](int, const QString&) {
        ADDMSG(ElaMessageBarType::Top, "备注已更新", this, 1, 2000);
    });

    // 加好友策略保存失败提示（成功静默）
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_set_policy_rsp, this, [this](int error) {
        if (error != 0) {
            ADDMSG(ElaMessageBarType::Top, "加好友策略保存失败", this, 0, 3000);
        }
    });

    // 连接状态进状态栏：断线提示重连中，恢复后复位
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_link_state, this, [this](bool online) {
        if (online) {
            SET_STATUS(QString("欢迎回来,%1! ").arg(NAME));
        }
        else {
            SET_STATUS("连接已断开…");
        }
    });

    // AI 窗口工具执行（同线程直连，返回即视为成功）
    connect(AiMgr::GetInstance().get(), &AiMgr::sig_window_op, this,
            &C_Window::slot_window_op, Qt::DirectConnection);

    // 删除好友：回包与对端通知汇入同一套 UI 移除流程
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_delete_friend_rsp,
            this, &C_Window::slot_delete_friend_rsp);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_delete_friend_notify,
            this, &C_Window::slot_delete_friend_notify);

    // ===== 右下角消息通知弹窗：仅主窗口非前台时提示 =====
    _notifyPopup = new NotifyPopup(this);
    connect(_notifyPopup, &NotifyPopup::sigNoticeClicked,
            this, &C_Window::slot_notice_clicked);

    // 新聊天消息（ChatDialog 连同一信号做列表/未读，互不干扰）
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_text_chat_msg,
            this, &C_Window::slot_popup_text_chat_msg);
    // 对方通过我的好友申请
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_auth_friend,
            this, &C_Window::slot_popup_add_auth_friend);

    // 视频来电：主窗口不在前台（含最小化）时右下角弹卡片，点击唤起来电弹窗
    connect(CallManager::GetInstance().get(), &CallManager::sig_incoming_call,
            this, [this](int fromuid, QString callId) {
                Q_UNUSED(callId);
                if (!shouldPopupNotice()) {
                    return;
                }
                auto fi = UserMgr::GetInstance()->GetFriendById(fromuid);
                const QString title = fi ? fi->DisplayName() : QString::number(fromuid);
                const QString icon = fi ? fi->_icon : QString();
                const QString body =
                    (CallManager::GetInstance()->callType() == CallType::VOICE)
                        ? QStringLiteral("邀请你进行语音通话")
                        : QStringLiteral("邀请你进行视频通话");
                _notifyPopup->notify(NotifyPopup::IncomingCall, fromuid, title, body, icon);
            });

    // ===== 通话系统消息写入对话（微信式：拒接/通话时长，本地各写各的）=====
    connect(CallManager::GetInstance().get(), &CallManager::sig_rejected_by_self,
            this, [this](int peerUid) {
                if (_chatDialog) {
                    _chatDialog->AppendLocalCallMsg(peerUid, true,
                                                    QStringLiteral("已拒绝通话"));
                }
            });
    connect(CallManager::GetInstance().get(), &CallManager::sig_call_rejected,
            this, [this](int peerUid) {
                if (_chatDialog) {
                    _chatDialog->AppendLocalCallMsg(peerUid, false,
                                                    QStringLiteral("对方已拒绝通话"));
                }
            });
    connect(CallManager::GetInstance().get(), &CallManager::sig_call_ended,
            this, [this](int reason, int durationSec, int peerUid) {
                Q_UNUSED(reason);
                if (durationSec <= 0 || peerUid <= 0 || !_chatDialog) {
                    return; // 未接通的结束不写记录
                }
                const QString text =
                    durationSec < 60
                        ? QStringLiteral("通话时长 %1秒").arg(durationSec)
                        : QStringLiteral("通话时长 %1分%2秒")
                              .arg(durationSec / 60)
                              .arg(durationSec % 60);
                _chatDialog->AppendLocalCallMsg(peerUid,
                                                CallManager::GetInstance()->asCaller(), text);
            });

    // 好友/会话列表刷新回包：消息页与通讯录页共用一个请求，统一在此提示
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friend_list_refreshed, this,
            [this](bool ok) {
                ADDMSG(ElaMessageBarType::Top, ok ? "刷新成功" : "刷新失败",
                       this, ok ? 1 : 0, 2000);
            });
}

C_Window::~C_Window()
{

}

void C_Window::initStatus()
{
    //状态栏
    ElaStatusBar* statusBar = new ElaStatusBar(this);
    _statusText = STAUTUS();
    // _statusText->setTextPixelSize(14);
    statusBar->addWidget(_statusText);
    // _statusText->setMaximumWidth(800);
    auto userInfo = UserMgr::GetInstance()->GetUserInfo();
    SET_STATUS(QString("UID: %1   欢迎回来,%2! ")
                   .arg(UserMgr::GetInstance()->GetUid())
                   .arg(userInfo ? userInfo->DisplayName() : QString()));
    this->setStatusBar(statusBar);
    // 设置新状态 _statusText->setText("sadasd");
}

void C_Window::initToolBar()
{

}

void C_Window::initTray()
{
    _trayIcon = new QSystemTrayIcon(QIcon(":/icons/image.png"), this);
    _trayIcon->setToolTip("FoolChat");

    QMenu* trayMenu = new QMenu(this);
    QAction* showAct = trayMenu->addAction(QStringLiteral("显示主窗口"));
    trayMenu->addSeparator();
    QAction* logoutAct = trayMenu->addAction(QStringLiteral("退出登录"));
    QAction* quitAct = trayMenu->addAction(QStringLiteral("退出"));

    auto showWindow = [this]() {
        showNormal();
        raise();
        activateWindow();
    };
    connect(showAct, &QAction::triggered, this, showWindow);
    connect(_trayIcon, &QSystemTrayIcon::activated, this, [this, showWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showWindow();
        }
    });
    connect(logoutAct, &QAction::triggered, this, [this]() {
        emit sigLogoutRequested();
    });
    connect(quitAct, &QAction::triggered, this, []() {
        qApp->quit();
    });

    _trayIcon->setContextMenu(trayMenu);
    _trayIcon->show();
}

void C_Window::updateTrayBadge()
{
    if (_trayIcon == nullptr) {
        return;
    }
    const int total = _chatUnread + _noticeUnread + _contactMsg;
    QIcon base(":/icons/image.png");
    if (total <= 0) {
        _trayIcon->setIcon(base);
        return;
    }
    // 未读角标绘制到托盘图标右上角
    QPixmap pix = base.pixmap(32, 32);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor("#ff4d4f"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(24, 8), 8, 8);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPointSize(8);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(14, 0, 20, 16), Qt::AlignCenter, total > 99 ? QStringLiteral("99+") : QString::number(total));
    _trayIcon->setIcon(QIcon(pix));
}

void C_Window::initWindow()
{
    setIsAllowPageOpenInNewWindow(true);
    setUserInfoCardPixmap(QPixmap(UserMgr::GetInstance()->GetIcon()));  //卡片头像
    // 卡片昵称显示 nick，未设置过 nick 的账号回退为账号名
    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    QString nick = user_info ? user_info->_nick : "";
    setUserInfoCardTitle(nick.isEmpty() ? QString("%1").arg(NAME) : nick);   //卡片昵称
    setUserInfoCardSubTitle(QString("%1").arg(EMAIL));   //卡片邮箱
    setWindowTitle("Fool Chat");
    setIsFixedSize(false);      // ElaWindow 默认固定大小，显式放开缩放
    setMinimumSize(1024, 640);  // 限制最小尺寸
    resize(1200, 680);          // 默认尺寸
    setNavigationBarWidth(225); //导航栏宽度
    setWindowIcon(QIcon(":/icons/image.png"));    //窗口icon
}

void C_Window::initContent()
{

}

void C_Window::initClose()
{
    ElaContentDialog* closeConfirmDialog = new ElaContentDialog(this);
    closeConfirmDialog->resize(500, 250);
    closeConfirmDialog->setStyleSheet(
        "ElaContentDialog {"
        "   background: transparent;"
        "   border-radius: 12px;"
        "}"
    );
    closeConfirmDialog->setLeftButtonText("取消");
    closeConfirmDialog->setMiddleButtonText("最小化");
    closeConfirmDialog->setRightButtonText("关闭");

    QWidget* customWidget = new QWidget(closeConfirmDialog);
    QVBoxLayout* layout = new QVBoxLayout(customWidget);
    layout->setContentsMargins(20, 30, 20, 10);

    ElaText* title = new ElaText("关闭窗口", closeConfirmDialog);
    title->setTextPixelSize(36);
    title->setTextStyle(ElaTextType::Title);

    ElaText* subTitle = new ElaText("确定要关闭窗口吗", closeConfirmDialog);
    subTitle->setTextStyle(ElaTextType::Body);

    layout->addWidget(title);
    layout->addSpacing(15);
    layout->addWidget(subTitle);
    layout->addStretch();

    closeConfirmDialog->setCentralWidget(customWidget);

    connect(closeConfirmDialog, &ElaContentDialog::rightButtonClicked, this, &C_Window::closeWindow);
    connect(closeConfirmDialog, &ElaContentDialog::middleButtonClicked, this, [=]() {
        closeConfirmDialog->close();
        showMinimized();
    });
    connect(this, &C_Window::closeButtonClicked, this, [=]() {
        closeConfirmDialog->exec();
    });

    // 禁用默认关闭
    setIsDefaultClosed(false);
}

void C_Window::initGropMember()
{
    // todo...
}

void C_Window::initGropAnnouncement()
{
    // todo...
}

void C_Window::initNav()
{
    QString chatKey,starKey,aboutKey,adminKey;
    _contactDialog = new ContactDialog(this);
    _chatDialog = new ChatDialog(this);
    _noticePage = new NoticePage(this);
    _dynamicPage = new Dynamic_Page(this);
    // 冷门页懒加载：音乐盒/编辑器先占位，首次点击导航再构建，缩短首开时间
    QWidget* musicHolder = new QWidget(this);
    QWidget* editorHolder = new QWidget(this);

    // 连接通讯录跳转聊天信号
    connect(_contactDialog, &ContactDialog::sig_jump_chat_item,
            _chatDialog, &ChatDialog::slot_jump_chat_item_from_user_info);
    // 连接切换到聊天页面信号
    connect(_contactDialog, &ContactDialog::sig_switch_to_chat_page,
            this, &C_Window::slot_switch_to_chat_page);

    addExpanderNode("聊天通讯", chatKey, ElaIconType::MessageDots);
    addPageNode("消息", _chatDialog, chatKey, ElaIconType::Comments);
    addPageNode("通讯录", _contactDialog, chatKey, ElaIconType::FileUser);
    addPageNode("通知", _noticePage, chatKey, ElaIconType::Bell);

    addExpanderNode("社交动态", starKey, ElaIconType::CalendarStar);
    addPageNode("代码编辑器(娱乐)", editorHolder, starKey,ElaIconType::Code);
    addPageNode("音乐盒", musicHolder, starKey, ElaIconType::Music);
    addPageNode("心情树洞", _dynamicPage, starKey, ElaIconType::Star);

    // 首次导航到冷门页时构建真实页面
    connect(this, &ElaWindow::navigationNodeClicked, this,
            [this, musicHolder, editorHolder](ElaNavigationType::NavigationNodeType, QString nodeKey) {
        if (_musicPage == nullptr && nodeKey == musicHolder->property("ElaPageKey").toString()) {
            _musicPage = new MusicPage(musicHolder);
            auto* lay = new QVBoxLayout(musicHolder);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->addWidget(_musicPage);
        }
        if (_editorPage == nullptr && nodeKey == editorHolder->property("ElaPageKey").toString()) {
            _editorPage = new EditorPage(editorHolder);
            auto* lay = new QVBoxLayout(editorHolder);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->addWidget(_editorPage);
        }
    });
    addFooterNode("关于", nullptr,aboutKey, 0, ElaIconType::User);
    _aboutPage = new C_About();

    _aboutPage->hide();
    connect(this, &ElaWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) {
        if (aboutKey == nodeKey)
        {
            _aboutPage->moveToCenter();
            _aboutPage->show();
        }
    });

    // root账号后台管理
    AdminWid* adminPage = new AdminWid(this);
    addFooterNode("后台管理(网页测试)",adminPage,adminKey, 0, ElaIconType::Family);

    QString settingKey;
    _settingPage = new F_Setting(this);
    addFooterNode("设置", _settingPage, settingKey, 0, ElaIconType::Gear);

    // 用户卡片点击弹菜单：编辑资料 / 设置 / 退出登录
    connect(this, &ElaWindow::userInfoCardClicked, this, [this, settingKey]() {
        ElaMenu menu(this);
        QAction* profileAct = menu.addAction(QStringLiteral("编辑资料"));
        QAction* settingAct = menu.addAction(QStringLiteral("设置"));
        menu.addSeparator();
        QAction* logoutAct = menu.addAction(QStringLiteral("退出登录"));

        QAction* chosen = menu.exec(QCursor::pos());
        if (chosen == profileAct) {
            auto* dlg = new EditProfileDlg(this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        }
        else if (chosen == settingAct) {
            navigation(settingKey);
        }
        else if (chosen == logoutAct) {
            emit sigLogoutRequested();
        }
    });

    // 展开导航
    expandNavigationNode(chatKey);
    expandNavigationNode(starKey);

    connect(_noticePage,&NoticePage::sig_num_msg,this,&C_Window::Slot_Set_Msg_Num);

    // 聊天页未读总数 → 导航"聊天"节点红点 + 托盘角标
    connect(_chatDialog, &ChatDialog::sig_chat_unread, this, [this](int total) {
        setMsgNum(_chatDialog, total);
        _chatUnread = total;
        updateTrayBadge();
    });

    // 头像点击跳好友资料页：切到通讯录并展示该好友
    connect(_chatDialog, &ChatDialog::sig_show_friend_profile, this, [this](int uid) {
        navigation(_contactDialog->property("ElaPageKey").toString());
        _contactDialog->ShowFriendInfo(uid);
    });

    // 点"消息"导航节点：有未读则直达第一个未读会话
    connect(this, &ElaWindow::navigationNodeClicked, this,
            [this](ElaNavigationType::NavigationNodeType, QString nodeKey) {
        if (nodeKey == _chatDialog->property("ElaPageKey").toString()) {
            _chatDialog->openFirstUnread();
        }
    });

    // 离线期间积压的好友申请：按未处理数补角标
    refreshApplyBadge();
    updateTrayBadge();

    // 测试信号链接
    _noticePage->loadData();

    // AI open_page 工具映射：页面节点用 widget 属性里的 key，footer 节点用创建时输出的 key
    _aiPageMap.insert("chat", _chatDialog->property("ElaPageKey").toString());
    _aiPageMap.insert("contacts", _contactDialog->property("ElaPageKey").toString());
    _aiPageMap.insert("notice", _noticePage->property("ElaPageKey").toString());
    _aiPageMap.insert("dynamics", _dynamicPage->property("ElaPageKey").toString());
    _aiPageMap.insert("music", musicHolder->property("ElaPageKey").toString());
    _aiPageMap.insert("editor", editorHolder->property("ElaPageKey").toString());
    _aiPageMap.insert("setting", settingKey);
    _aiPageMap.insert("settings", settingKey);
    _aiPageMap.insert("admin", adminKey);
}

void C_Window::Slot_Set_Msg_Num(QWidget *p, int n)
{
    setMsgNum(p,n);
    if (p == _noticePage) {
        _noticeUnread = n;
        updateTrayBadge();
    }
}

void C_Window::slot_apply_friend(std::shared_ptr<AddFriendApply> apply)
{
    // 不做前置拦截：删好友后的重新申请要能再次进来，条目去重由 AddNewApply 负责
    UserMgr::GetInstance()->AddApplyList(std::make_shared<ApplyInfo>(apply));
    refreshApplyBadge();
    ADDMSG(ElaMessageBarType::Top,"您有新的好友申请！",this,2);
    emit SigContactApply(apply);

    // 窗口内提示由上方 ADDMSG 负责，非前台时右下角补一张卡片
    if (shouldPopupNotice()) {
        const QString title = apply->_nick.isEmpty() ? apply->_name : apply->_nick;
        const QString body = apply->_desc.isEmpty()
                                 ? QStringLiteral("请求添加你为好友")
                                 : apply->_desc;
        _notifyPopup->notify(NotifyPopup::FriendApply, apply->_from_uid,
                             title, body, apply->_icon);
    }
}

void C_Window::refreshApplyBadge()
{
    _contactMsg = UserMgr::GetInstance()->PendingApplyCount();
    setMsgNum(_contactDialog, _contactMsg);
}

void C_Window::slot_switch_to_chat_page()
{
    this->navigation(_chatDialog->property("ElaPageKey").toString());
}

void C_Window::slot_window_op(const QString& op, const QString& arg)
{
    if (op == QLatin1String("open_page")) {
        if (arg == QLatin1String("about")) {
            if (_aboutPage) {
                _aboutPage->moveToCenter();
                _aboutPage->show();
            }
            return;
        }
        const QString pageKey = _aiPageMap.value(arg);
        if (!pageKey.isEmpty()) {
            navigation(pageKey);
        }
        return;
    }
    if (op == QLatin1String("set_font_size")) {
        QFont font = qApp->font();
        font.setPointSize(arg == QLatin1String("small") ? 8 : (arg == QLatin1String("large") ? 12 : 9));
        qApp->setFont(font);
        ADDMSG(ElaMessageBarType::Top, "字体大小已调整", this, 1, 2000);
        return;
    }
    if (op == QLatin1String("edit_remark")) {
        const QStringList parts = arg.split('|');
        if (parts.size() != 2) {
            return;
        }
        // 按显示名（备注>昵称>账号名）或账号名找好友
        for (auto& f : UserMgr::GetInstance()->GetConListPerPage()) {
            if (f->DisplayName() == parts.at(0) || f->_name == parts.at(0)) {
                QJsonObject obj;
                obj["uid"] = UserMgr::GetInstance()->GetUid();
                obj["touid"] = f->_uid;
                obj["back"] = parts.at(1).trimmed();
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_UPDATE_BACK_REQ,
                                                         QJsonDocument(obj).toJson(QJsonDocument::Compact));
                return;
            }
        }
        return;
    }
    if (op == QLatin1String("clear_cache")) {
        if (arg == QLatin1String("wallpaper")) {
            const QString dir = QCoreApplication::applicationDirPath() + "/wallpaper";
            QDir(dir).removeRecursively();
            QDir().mkpath(dir);
        }
        else if (arg == QLatin1String("music")) {
            if (QSqlDatabase::contains("music_sqlite")) {
                QSqlDatabase musicDb = QSqlDatabase::database("music_sqlite");
                if (musicDb.isOpen()) {
                    QSqlQuery q(musicDb);
                    q.exec("DELETE FROM songs");
                }
            }
        }
        ADDMSG(ElaMessageBarType::Top, "缓存已清理", this, 1, 2000);
        return;
    }
    if (op == QLatin1String("window_minimize")) {
        showMinimized();
        return;
    }
    if (op == QLatin1String("window_center")) {
        if (QScreen* screen = QGuiApplication::primaryScreen()) {
            const QRect ag = screen->availableGeometry();
            move(ag.center() - rect().center());
        }
    }
}

template<typename T>
void C_Window::setMsgNum(T t, int n)
{
    QString key = t->property("ElaPageKey").toString();
    setNodeKeyPoints(key,n);
}

void C_Window::removeFriendUi(int uid)
{
    UserMgr::GetInstance()->RemoveFriend(uid);
    _chatDialog->slot_remove_chat_user(uid);
    _contactDialog->slot_remove_contact_user(uid);
    refreshApplyBadge();
    updateTrayBadge();
}

void C_Window::slot_delete_friend_rsp(int uid, int error)
{
    if (error != ErrorCodes::SUCCESS) {
        ADDMSG(ElaMessageBarType::Top, "删除好友失败", this, 0, 3000);
        return;
    }
    removeFriendUi(uid);
    ADDMSG(ElaMessageBarType::Top, "已删除该好友", this, 1, 3000);
}

void C_Window::slot_delete_friend_notify(int fromuid)
{
    // 先取昵称再移除（移除后 map 查不到）
    auto fi = UserMgr::GetInstance()->GetFriendById(fromuid);
    const QString name = fi ? fi->_name : QString::number(fromuid);
    removeFriendUi(fromuid);
    ADDMSG(ElaMessageBarType::Top, QString("你与 %1 已不是好友").arg(name), this, 2, 3000);
}

bool C_Window::shouldPopupNotice() const
{
    // 设置页"消息通知"总开关（默认开），关掉则右下角一律不弹
    QSettings uiSettings(QCoreApplication::applicationDirPath() + "/ui_settings.ini",
                         QSettings::IniFormat);
    if (!uiSettings.value("notifyOn", true).toBool()) {
        return false;
    }
    // 最小化时 Windows 可能仍把本窗标为活动窗，必须先单独判
    if (isMinimized()) {
        return true;
    }
    // activeWindow() 只返回本应用的活动窗口：null = 焦点在其它应用 → 该弹
    QWidget* active = QApplication::activeWindow();
    if (active == nullptr) {
        return true;
    }
    // 活动窗口是主窗口本身或其子孙（对话框均以 this 为父）→ 前台使用中，不弹
    for (QWidget* w = active; w != nullptr; w = w->parentWidget()) {
        if (w == this) {
            return false;
        }
    }
    return true;
}

void C_Window::slot_popup_text_chat_msg(std::shared_ptr<TextChatMsg> msg)
{
    if (!msg || msg->_chat_msgs.empty()) {
        return;
    }
    // 自己发出的回显不通知（与 ChatDialog 的守卫一致）
    if (msg->_from_uid == UserMgr::GetInstance()->GetUid()) {
        return;
    }
    if (!shouldPopupNotice()) {
        return;
    }
    // 窗口在屏且正打开发信人会话 → 消息已直接可见，不弹
    if (!isMinimized() && isVisible() && _chatDialog->isVisible()
        && _chatDialog->currentChatUid() == msg->_from_uid) {
        return;
    }
    auto fi = UserMgr::GetInstance()->GetFriendById(msg->_from_uid);
    const QString title = fi ? fi->DisplayName() : QString::number(msg->_from_uid);
    const QString icon = fi ? fi->_icon : QString();
    _notifyPopup->notify(NotifyPopup::TextChat, msg->_from_uid, title,
                         msg->_chat_msgs.back()->_msg_content, icon);
}

void C_Window::slot_popup_add_auth_friend(std::shared_ptr<AuthInfo> auth)
{
    if (!auth || !shouldPopupNotice()) {
        return;
    }
    const QString title = auth->_nick.isEmpty() ? auth->_name : auth->_nick;
    _notifyPopup->notify(NotifyPopup::AuthFriend, auth->_uid, title,
                         QStringLiteral("已通过你的好友申请"), auth->_icon);
}

void C_Window::slot_notice_clicked(int kind, int uid)
{
    // 卡片点击会激活卡片小窗，先把焦点收回主窗口（托盘 showWindow 同款三连）
    showNormal();
    raise();
    activateWindow();

    // 来电卡片：主窗口最小化时来电弹窗是其从属窗也处于最小化，需单独唤起
    if (kind == NotifyPopup::IncomingCall) {
        if (auto* dlg = findChild<ElaDialog*>("IncomingCallDlg")) {
            dlg->showNormal();
            dlg->raise();
            dlg->activateWindow();
        }
        return;
    }

    if (kind == NotifyPopup::FriendApply) {
        navigation(_contactDialog->property("ElaPageKey").toString());
        // 申请子页只有 private slot，经 meta-object 调用；失败则停留在通讯录
        QMetaObject::invokeMethod(_contactDialog, "slot_switch_apply_friend_page");
        return;
    }

    // TextChat / AuthFriend：打开与该 uid 的会话
    navigation(_chatDialog->property("ElaPageKey").toString());
    auto fi = UserMgr::GetInstance()->GetFriendById(uid);
    if (fi == nullptr) {
        // 好友已被删除等异常：仅激活主窗口，不跳会话
        return;
    }
    _chatDialog->slot_jump_chat_item_from_user_info(std::make_shared<UserInfo>(fi));
}
