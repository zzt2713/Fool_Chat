#include "conuseritem.h"
#include "ui_conuseritem.h"
#include "themedtext.h"
#include "friendops.h"
#include "pixmaputil.h"

ConUserItem::ConUserItem(QWidget *parent)
    : ListItemBase(parent)
    , ui(new Ui::ConUserItem)
{
    ui->setupUi(this);
    // ElaText 颜色绑定主题（自带样式表不吃 palette 更新）
    ui->user_name_lb->setTextStyle(ElaTextType::Body);
    BindTextToTheme(ui->user_name_lb);
    // 默认按联系人条目处理（AI/新的朋友创建后会覆盖），否则动态新增的条目点击无效
    SetItemType(ListItemType::CONTACT_USER_ITEM);
    // Status_label 构造里写死 20x20，这里改小为紧凑状态点
    ui->status_lb->setFixedSize(12, 12);

    // 右键弹出"删除好友"菜单（仅真实好友：AI 的 -1、"新的朋友"的 0 都不弹）
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto info = GetInfo();
        if (!info || info->_uid <= 0) {
            return;
        }
        PopFriendContextMenu(this, pos, info->_uid);
    });
}

ConUserItem::~ConUserItem()
{
    delete ui;
}

QSize ConUserItem::sizeHint() const
{
    return QSize(250,70);
}

void ConUserItem::SetInfo(std::shared_ptr<AuthInfo> auth_info)
{
    _info = std::make_shared<UserInfo>(auth_info);
    QPixmap pixmap(_info->_icon);
    qreal dpr = this->devicePixelRatioF();

    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, ui->icon_lb->size(), dpr));
    ui->icon_lb->setScaledContents(false);
    ui->user_name_lb->setText(_info->DisplayName());
    updateStatus(_info->_status);
}

void ConUserItem::SetInfo(std::shared_ptr<FriendInfo> friend_info)
{
    _info = std::make_shared<UserInfo>(friend_info);
    QPixmap pixmap(_info->_icon);

    qreal dpr = this->devicePixelRatioF();

    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, ui->icon_lb->size(), dpr));
    ui->icon_lb->setScaledContents(false);
    ui->user_name_lb->setText(_info->DisplayName());
    updateStatus(_info->_status);
}

void ConUserItem::SetInfo(std::shared_ptr<AuthRsp> auth_info)
{
    _info = std::make_shared<UserInfo>(auth_info);

    QPixmap pixmap(_info->_icon);

    qreal dpr = this->devicePixelRatioF();

    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, ui->icon_lb->size(), dpr));
    ui->icon_lb->setScaledContents(false);
    ui->user_name_lb->setText(_info->DisplayName());
    updateStatus(_info->_status);
}

void ConUserItem::SetInfo(int uid, QString name, QString icon)
{
    _info = std::make_shared<UserInfo>(uid,name,icon);

    QPixmap pixmap(_info->_icon);

    qreal dpr = this->devicePixelRatioF();

    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, ui->icon_lb->size(), dpr));
    ui->icon_lb->setScaledContents(false);
    ui->user_name_lb->setText(_info->_name);
    updateStatus(_info->_status);
}

std::shared_ptr<UserInfo> ConUserItem::GetInfo()
{
    return _info;
}

void ConUserItem::ShowRedPoint(bool show)
{
    if(show){
        ui->red_lb->show();
    }else{
        ui->red_lb->hide();
    }
}

void ConUserItem::addNewFriend()
{
    _info = std::make_shared<UserInfo>(0,"新的朋友","");
    ui->icon_lb->setElaIcon(ElaIconType::UserPlus);

    ui->user_name_lb->setText(_info->_name);
    // "新的朋友"是功能入口不是好友，不显示状态点
    ui->status_lb->hide();
}

void ConUserItem::SetStatus(int status)
{
    if (_info) {
        _info->_status = status;
    }
    updateStatus(status);
}

void ConUserItem::RefreshDisplay()
{
    if (_info) {
        ui->user_name_lb->setText(_info->DisplayName());
    }
}

void ConUserItem::updateStatus(int status)
{
    // AI 助手恒为在线，其余按状态字段（0离线 1在线）
    bool online = (_info && _info->_uid == -1) || status == 1;
    ui->status_lb->setStatus(online);
}
