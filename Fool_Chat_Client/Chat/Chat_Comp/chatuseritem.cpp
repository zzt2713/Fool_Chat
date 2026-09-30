#include "chatuseritem.h"
#include "ui_chatuseritem.h"
#include "themedtext.h"
#include "friendops.h"
#include "../Chat_Page/chatdialog.h"
#include "pixmaputil.h"
#include <QPixmap>
#include <QFontMetrics>

// 超宽文本省略号截断（完整文本由调用方放悬停提示）
static QString elidedForItem(const QString& text, const QFont& font, int width)
{
    return QFontMetrics(font).elidedText(text, Qt::ElideRight, width);
}

// 沿父链找到聊天页（置顶/清记录是列表层的动作）
static ChatDialog* FindChatDialog(QWidget* from)
{
    for (QWidget* w = from; w != nullptr; w = w->parentWidget()) {
        if (auto* dlg = qobject_cast<ChatDialog*>(w)) {
            return dlg;
        }
    }
    return nullptr;
}

ChatUseritem::ChatUseritem(QWidget *parent)
    : ListItemBase(parent)
    , ui(new Ui::ChatUseritem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::CHAT_USER_ITEM);
    // ElaText 颜色绑定主题（自带样式表不吃 palette 更新）
    ui->user_name->setTextStyle(ElaTextType::Body);
    ui->user_chat_lib->setTextStyle(ElaTextType::Body);
    ui->time_lb->setTextStyle(ElaTextType::Body);
    BindTextToTheme(ui->user_name);
    BindTextToTheme(ui->time_lb);
    // 最后一条消息预览是次级文字：淡灰/淡白，不随主文字黑白切
    BindMutedTextToTheme(ui->user_chat_lib);
    ui->red_dot_lb->setVisible(false);
    ui->icon_lb->installEventFilter(this);

    // 消息页右键菜单：置顶/取消置顶 + 删除聊天记录（AI 的 uid=-1 不出菜单）
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto info = GetUserInfo();
        if (!info || info->_uid == -1) {
            return;
        }
        ChatDialog* dlg = FindChatDialog(this);
        if (!dlg) {
            return;
        }

        ElaMenu menu(this);
        const bool pinned = UserMgr::GetInstance()->IsPinned(info->_uid);
        QAction* pinAction = menu.addAction(pinned ? QStringLiteral("取消置顶")
                                                    : QStringLiteral("置顶"));
        QAction* clearAction = menu.addAction(QStringLiteral("删除聊天记录"));

        connect(pinAction, &QAction::triggered, this, [dlg, uid = info->_uid]() {
            dlg->togglePin(uid);
        });
        connect(clearAction, &QAction::triggered, this, [this, dlg, uid = info->_uid]() {
            ConfirmAction(this, "删除聊天记录",
                          "删除后将无法恢复与该好友的聊天内容，确定删除吗", "删除",
                          [dlg, uid]() { dlg->clearChatHistory(uid); });
        });
        menu.exec(mapToGlobal(pos));
    });
}

ChatUseritem::~ChatUseritem()
{
    delete ui;
}

bool ChatUseritem::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->icon_lb && event->type() == QEvent::MouseButtonPress) {
        auto info = GetUserInfo();
        if (info && info->_uid > 0) {
            if (ChatDialog* dlg = FindChatDialog(this)) {
                dlg->showFriendProfile(info->_uid);
            }
        }
        return true;
    }
    return ListItemBase::eventFilter(watched, event);
}

QSize ChatUseritem::sizeHint() const
{
    return QSize(250,70);
}

void ChatUseritem::SetInfo(QString name, QString head, QString msg)
{
    _user_info = std::make_shared<UserInfo>(0, name, name, head, 0, msg);

    QPixmap pixmap;
    if (!pixmap.load(head)) {
        pixmap.load(":/default_avatar.png");
    }

    QSize iconSize = ui->icon_lb->size();
    if (iconSize.isEmpty() || iconSize.width() <= 0) {
        iconSize = QSize(45, 45);
    }
    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, iconSize, this->devicePixelRatioF()));
    ui->icon_lb->setScaledContents(false);

    ui->user_name->setText(_user_info->_name);
    ui->user_chat_lib->setText(_user_info->_last_msg);
}

void ChatUseritem::SetInfo(std::shared_ptr<FriendInfo> friend_info)
{
    _user_info = std::make_shared<UserInfo>(friend_info);
    // 头像走高保真圆图，避免二次缩放发糊
    QPixmap pixmap(_user_info->_icon);
    QSize iconSize = ui->icon_lb->size();
    if (iconSize.isEmpty() || iconSize.width() <= 0) {
        iconSize = QSize(45, 45);
    }
    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, iconSize, this->devicePixelRatioF()));
    ui->icon_lb->setScaledContents(false);

    ui->user_name->setText(elidedForItem(_user_info->DisplayName(), ui->user_name->font(), 180));
    ui->user_name->setToolTip(_user_info->DisplayName());
    ui->user_chat_lib->setText(elidedForItem(_user_info->_last_msg, ui->user_chat_lib->font(), 200));
    ui->user_chat_lib->setToolTip(_user_info->_last_msg);
}

QString ChatUseritem::getName()
{
    return _user_info->_name;
}

void ChatUseritem::SetInfo(std::shared_ptr<UserInfo> user_info)
{
    _user_info = user_info;
    // 头像走高保真圆图，避免二次缩放发糊
    QPixmap pixmap(_user_info->_icon);
    QSize iconSize = ui->icon_lb->size();
    if (iconSize.isEmpty() || iconSize.width() <= 0) {
        iconSize = QSize(45, 45);
    }
    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, iconSize, this->devicePixelRatioF()));
    ui->icon_lb->setScaledContents(false);

    ui->user_name->setText(elidedForItem(_user_info->DisplayName(), ui->user_name->font(), 180));
    ui->user_name->setToolTip(_user_info->DisplayName());
    ui->user_chat_lib->setText(elidedForItem(_user_info->_last_msg, ui->user_chat_lib->font(), 200));
    ui->user_chat_lib->setToolTip(_user_info->_last_msg);
}

std::shared_ptr<UserInfo> ChatUseritem::GetUserInfo()
{
    return _user_info;
}

void ChatUseritem::updateLastMsg(std::vector<std::shared_ptr<TextChatData>> msgs)
{
    QString last_msg = "";
    for(auto msg : msgs){
        last_msg = msg->_msg_content;
        _user_info->_chat_msgs.push_back(msg);
    }

    _user_info->_last_msg = last_msg;
    ui->user_chat_lib->setText(elidedForItem(_user_info->_last_msg, ui->user_chat_lib->font(), 200));
    ui->user_chat_lib->setToolTip(_user_info->_last_msg);
}

void ChatUseritem::SetUnread(int count)
{
    ui->red_dot_lb->setVisible(count > 0);
}
