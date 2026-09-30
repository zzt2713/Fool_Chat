#include "friendinfopage.h"
#include "ui_friendinfopage.h"
#include "friendops.h"
#include "callui.h"
#include "pixmaputil.h"
#include "themedtext.h"
#include <QDebug>

FriendInfoPage::FriendInfoPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FriendInfoPage),_user_info(nullptr)
{
    ui->setupUi(this);
    // ElaText 默认字号偏大，资料页标签钉死常规字号（ui 里行高只留了 30px）
    ui->name_lb2->setTextPixelSize(20);
    ui->nick_tip->setTextPixelSize(15);
    ui->nick_lb->setTextPixelSize(15);
    ui->bak_tip->setTextPixelSize(15);
    ui->bak_lb->setTextPixelSize(15);
    ui->desc_tip->setTextPixelSize(15);
    ui->desc_lb->setTextPixelSize(15);
    BindTextToTheme(ui->name_lb2);
    BindTextToTheme(ui->nick_tip);
    BindTextToTheme(ui->nick_lb);
    BindTextToTheme(ui->bak_tip);
    BindTextToTheme(ui->bak_lb);
    BindTextToTheme(ui->desc_tip);
    BindTextToTheme(ui->desc_lb);
    // "账号："等提示在 ui 里限宽 50px，15px 字号带字距放不下会折行：放开宽度、单行显示
    for (ElaText* tip : {ui->nick_tip, ui->bak_tip, ui->desc_tip}) {
        tip->setMaximumWidth(80);
        tip->setWordWrap(false);
    }
    // 账号/备注是短文本，同样不折行（签名保留 ui 里的自动换行以容纳长文本）
    ui->nick_lb->setWordWrap(false);
    ui->bak_lb->setWordWrap(false);
    // 名字列弹性优先（stretch=1），性别图标/弹簧让位，长昵称不换行
    ui->horizontalLayout_2->setStretch(0, 1);
    ui->msg_chat->setElaIcon(ElaIconType::Message);
    ui->video_chat->setElaIcon(ElaIconType::Video);
    ui->voice_chat->setElaIcon(ElaIconType::PhoneRotary);
    ui->delete_chat->setElaIcon(ElaIconType::TrashCan);
}

FriendInfoPage::~FriendInfoPage()
{
    delete ui;
}

void FriendInfoPage::SetInfo(std::shared_ptr<UserInfo> user_info)
{
    _user_info = user_info;
    // 头像走高保真圆图，避免二次缩放发糊
    QPixmap pixmap(user_info->_icon);
    ui->icon_lb->setPixmap(PixmapUtil::round(pixmap, ui->icon_lb->size(), devicePixelRatioF()));
    ui->icon_lb->setScaledContents(false);

    // 大标题显示昵称（nick，未设置回退账号名）；第二行是账号（数据库 name）
    ui->name_lb2->setText(user_info->_nick.isEmpty() ? user_info->_name : user_info->_nick);
    ui->nick_lb->setText(user_info->_name);
    ui->bak_lb->setText(user_info->_back);

    // 签名：没填时显示默认文案
    QString desc = user_info->_desc.trimmed();
    ui->desc_lb->setText(desc.isEmpty() ? QStringLiteral("这个人很懒，什么都没有留下") : desc);

    // 性别图标：1男 2女，其余清空
    if (user_info->_sex == 1) {
        QPixmap sexIcon(":/icons/male.png");
        ui->sex_lb->setPixmap(sexIcon.scaled(ui->sex_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else if (user_info->_sex == 2) {
        QPixmap sexIcon(":/icons/female.png");
        ui->sex_lb->setPixmap(sexIcon.scaled(ui->sex_lb->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else {
        ui->sex_lb->clear();
    }
}

void FriendInfoPage::on_msg_chat_clicked()
{
    emit sig_jump_chat_item(_user_info);
}

void FriendInfoPage::on_video_chat_clicked()
{
    TryStartCall(this, _user_info ? _user_info->_uid : -1, CallType::VIDEO);
}

void FriendInfoPage::on_voice_chat_clicked()
{
    TryStartCall(this, _user_info ? _user_info->_uid : -1, CallType::VOICE);
}

void FriendInfoPage::on_delete_chat_clicked()
{
    if (!_user_info) {
        return;
    }
    AskDeleteFriend(this, _user_info->_uid);
}
