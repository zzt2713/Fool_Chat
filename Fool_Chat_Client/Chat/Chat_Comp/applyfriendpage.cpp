#include "applyfriendpage.h"
#include "ui_applyfriendpage.h"
#include "../../src/core/tcpmgr.h"
#include "../../src/core/usermgr.h"
#include "applyfrienditem.h"
#include "applyfriend.h"
#include <QPaintEvent>
#include <QStyleOption>
#include <QRandomGenerator>
#include <QPainter>
#include "authenfriend.h"

ApplyFriendPage::ApplyFriendPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ApplyFriendPage)
{
    ui->setupUi(this);
    connect(ui->apply_friend_list, &ApplyFriendList::sig_show_search, this, &ApplyFriendPage::sig_show_search);
    loadApplyList();
    //接受tcp传递的authrsp信号处理
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_auth_rsp, this, &ApplyFriendPage::slot_auth_rsp);
    // 互申场景：对方同意了我的申请 → 我这边的反向申请条目也置已同意
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_auth_friend, this,
            [this](std::shared_ptr<AuthInfo> auth_info) { markApplyAccepted(auth_info->_uid); });
}

ApplyFriendPage::~ApplyFriendPage()
{
    delete ui;
}

void ApplyFriendPage::AddNewApply(std::shared_ptr<AddFriendApply> apply)
{
    auto apply_info = std::make_shared<ApplyInfo>(apply->_from_uid,
                                                  apply->_name, apply->_desc,apply->_icon, apply->_nick, 0, 0);
    // 同人重新申请：复用旧条目（含历史"已同意"），重置为待处理，避免出现重复用户
    for (int row = 0; row < ui->apply_friend_list->count(); ++row) {
        auto* exist_item = qobject_cast<ApplyFriendItem*>(
            ui->apply_friend_list->itemWidget(ui->apply_friend_list->item(row)));
        if (exist_item && exist_item->GetUid() == apply->_from_uid) {
            exist_item->SetInfo(apply_info);
            exist_item->ShowAddBtn(true);
            _unauth_items[apply->_from_uid] = exist_item;
            return;
        }
    }

    // 先模拟头像随机，以后头像资源增加资源服务器后再显示
    // int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    // int head_i = randomValue % head.size();
    auto* apply_item = new ApplyFriendItem();
    apply_item->SetInfo( apply_info);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(apply_item->sizeHint());
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
    ui->apply_friend_list->insertItem(0,item);
    ui->apply_friend_list->setItemWidget(item, apply_item);
    apply_item->ShowAddBtn(true);
    // 登记待处理条目：同意成功后才能按 uid 找到它更新为"已同意"
    _unauth_items[apply_info->_uid] = apply_item;
    //收到审核好友信号
    connect(apply_item, &ApplyFriendItem::sig_auth_friend, [this](std::shared_ptr<ApplyInfo> apply_info) {
               auto* authFriend = new AuthenFriend(this);
               authFriend->setModal(true);
               authFriend->SetApplyInfo(apply_info);
               authFriend->show();
    });
}

void ApplyFriendPage::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ApplyFriendPage::loadApplyList()
{
    auto apply_list = UserMgr::GetInstance()->GetApplyList();
    for(auto &apply: apply_list){
        // int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
        // int head_i = randomValue % head.size();
        auto* apply_item = new ApplyFriendItem();
        // apply->SetIcon(head[head_i]);
        apply_item->SetInfo(apply);
        QListWidgetItem* item = new QListWidgetItem;
        //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
        item->setSizeHint(apply_item->sizeHint());
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        ui->apply_friend_list->insertItem(0,item);
        ui->apply_friend_list->setItemWidget(item, apply_item);
        if(apply->_status){
            apply_item->ShowAddBtn(false);
        }else{
            apply_item->ShowAddBtn(true);
            auto uid = apply_item->GetUid();
            _unauth_items[uid] = apply_item;
        }

        //收到审核好友信号
        connect(apply_item, &ApplyFriendItem::sig_auth_friend, [this](std::shared_ptr<ApplyInfo> apply_info) {
                       auto* authFriend = new AuthenFriend(this);
                       authFriend->setModal(true);
                       authFriend->SetApplyInfo(apply_info);
                       authFriend->show();
        });
    }

    // 模拟假数据，创建QListWidgetItem，并设置自定义的widget
    // for(int i = 0; i < 13; i++){
    //     int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    //     int str_i = randomValue%str.size();
    //     int head_i = randomValue%head.size();
    //     int name_i = randomValue%name.size();

    //     auto *apply_item = new ApplyFriendItem();
    //     auto apply = std::make_shared<ApplyInfo>(0, name[name_i], str[str_i],
    //                                              head[head_i], name[name_i], 0, 1);
    //     apply_item->SetInfo(apply);
    //     QListWidgetItem *item = new QListWidgetItem;
    //     //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    //     item->setSizeHint(apply_item->sizeHint());
    //     item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
    //     ui->apply_friend_list->addItem(item);
    //     ui->apply_friend_list->setItemWidget(item, apply_item);
    //     //收到审核好友信号
    //     connect(apply_item, &ApplyFriendItem::sig_auth_friend, [this](std::shared_ptr<ApplyInfo> apply_info){
    //         auto *authFriend =  new AuthenFriend(this);
    //         authFriend->setModal(true);
    //         authFriend->SetApplyInfo(apply_info);
    //         authFriend->show();
    //     });
    // }
}

void ApplyFriendPage::RemoveApply(int uid)
{
    _unauth_items.erase(uid);
    for (int row = 0; row < ui->apply_friend_list->count(); ++row) {
        auto* apply_item = qobject_cast<ApplyFriendItem*>(
            ui->apply_friend_list->itemWidget(ui->apply_friend_list->item(row)));
        if (apply_item && apply_item->GetUid() == uid) {
            apply_item->deleteLater();
            delete ui->apply_friend_list->takeItem(row);
            return;
        }
    }
}

void ApplyFriendPage::markApplyAccepted(int uid)
{
    // 兜底扫列表找条目（不依赖登记表），找到即置为已同意
    for (int row = 0; row < ui->apply_friend_list->count(); ++row) {
        auto* apply_item = qobject_cast<ApplyFriendItem*>(
            ui->apply_friend_list->itemWidget(ui->apply_friend_list->item(row)));
        if (apply_item && apply_item->GetUid() == uid) {
            apply_item->ShowAddBtn(false);
            break;
        }
    }

    // 同步内存中的申请状态（重进页面时直接显示已同意）
    for (auto& apply : UserMgr::GetInstance()->GetApplyList()) {
        if (apply->_uid == uid) {
            apply->_status = 1;
            break;
        }
    }
    _unauth_items.erase(uid);
}

void ApplyFriendPage::ReloadApplyList()
{
    _unauth_items.clear();
    while (ui->apply_friend_list->count() > 0) {
        auto* item = ui->apply_friend_list->item(0);
        if (auto* w = ui->apply_friend_list->itemWidget(item)) {
            w->deleteLater();
        }
        delete ui->apply_friend_list->takeItem(0);
    }
    loadApplyList();
}

void ApplyFriendPage::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp)
{
    markApplyAccepted(auth_rsp->_uid);
}
