#include "contactuserlist.h"
#include "tcpmgr.h"
#include "../../src/core/global.h"
#include "listitembase.h"
#include "ElaTheme.h"
#include <QRandomGenerator>
#include <QLabel>
#include "conuseritem.h"
#include "grouptipitem.h"

namespace {
// 分组条背景随主题：亮色浅灰条、暗色深灰条（写死 #eaeaea 暗黑下是刺眼白条）
void BindGroupTipBackground(QWidget* w)
{
    auto apply = [w](ElaThemeType::ThemeMode mode) {
        const QString bg = (mode == ElaThemeType::Dark) ? QStringLiteral("#2B2B2B")
                                                        : QStringLiteral("#eaeaea");
        w->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(bg));
    };
    apply(eTheme->getThemeMode());
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, w, apply);
}
} // namespace

ContactUserList::ContactUserList(QWidget *parent):QListWidget(parent),_load_pending(false)
{
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);

    //模拟从数据库或者后端传输过来的数据,进行列表加载
    addContactUserList();
    //连接点击的信号和槽
    connect(this, &QListWidget::itemClicked, this, &ContactUserList::slot_item_clicked);
     //链接对端同意认证后通知的信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_auth_friend,this,
       &ContactUserList::slot_add_auth_firend);

    //链接自己点击同意认证后界面刷新
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_auth_rsp,this,
       &ContactUserList::slot_auth_rsp);

    //好友上下线实时刷新状态点
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friend_status, this,
       &ContactUserList::slot_friend_status);

    // 备注修改成功：同步条目快照的 _back 后刷新显示名
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_back_updated, this,
       [this](int uid, const QString& back) {
        for (int row = 0; row < count(); ++row) {
            auto* conItem = qobject_cast<ConUserItem*>(itemWidget(item(row)));
            if (conItem && conItem->GetInfo() && conItem->GetInfo()->_uid == uid) {
                conItem->GetInfo()->_back = back;
                conItem->RefreshDisplay();
                return;
            }
        }
    });

    // 空列表占位提示（AI 入口恒在，好友区为空时仍提示）
    _emptyLabel = new QLabel("暂无更多联系人，点击右上角添加好友", this->viewport());
    _emptyLabel->setAlignment(Qt::AlignCenter);
    _emptyLabel->setStyleSheet("color: rgba(128, 128, 128, 0.9); font-size: 14px;");
    _emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(model(), &QAbstractItemModel::rowsInserted, this, [this]() { updateEmptyLabel(); });
    connect(model(), &QAbstractItemModel::rowsRemoved, this, [this]() { updateEmptyLabel(); });
    updateEmptyLabel();
    }

void ContactUserList::updateEmptyLabel()
{
    _emptyLabel->setGeometry(viewport()->rect());
    // 列表恒有"新的朋友"入口，只有条目全空（加载前）才提示
    _emptyLabel->setVisible(count() == 0);
}

void ContactUserList::resizeEvent(QResizeEvent* event)
{
    QListWidget::resizeEvent(event);
    updateEmptyLabel();
}

void ContactUserList::ShowRedPoint(bool bshow)
{
    _add_friend_item->ShowRedPoint(bshow);
}

ContactUserList::~ContactUserList()
{

}

bool ContactUserList::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this->viewport()) {
        if (event->type() == QEvent::Enter) {
            // 鼠标悬浮，显示滚动条
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        } else if (event->type() == QEvent::Leave) {
            // 鼠标离开，隐藏滚动条
            this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

    // 检查事件是否是鼠标滚轮事件
    if (watched == this->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        int numDegrees = wheelEvent->angleDelta().y() / 8;
        int numSteps = numDegrees / 15; // 计算滚动步数

        // 设置滚动幅度
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->value() - numSteps);

        // 检查是否滚动到底部
        QScrollBar *scrollBar = this->verticalScrollBar();
        int maxScrollValue = scrollBar->maximum();
        int currentValue = scrollBar->value();
        //int pageSize = 10; // 每页加载的联系人数量

        if (maxScrollValue - currentValue <= 0) {
            auto b_loaded = UserMgr::GetInstance()->IsLoadChatFin();
            if(b_loaded){
                return true;
            }
            if(_load_pending){
                return true;
            }

            _load_pending = true;

            QTimer::singleShot(100, [this](){
                _load_pending = false;
                QCoreApplication::quit();
            });
            // 滚动到底部，加载新的联系人
            //发送信号通知聊天界面加载更多聊天内容
            emit sig_loading_contact_user();
        }

        return true; // 停止事件传递
    }

    return QListWidget::eventFilter(watched, event);
}

void ContactUserList::addContactUserList()
{
    // AI 机器人入口：与消息页统一置顶（uid=-1），先插入即占 index0
    auto *ai_wid = new ConUserItem();
    ai_wid->SetInfo(-1, "AI助手", ":/icons/image.png");
    ai_wid->SetItemType(ListItemType::CONTACT_USER_ITEM);
    QListWidgetItem *ai_item = new QListWidgetItem;
    ai_item->setSizeHint(ai_wid->sizeHint());
    this->addItem(ai_item);
    this->setItemWidget(ai_item, ai_wid);

    auto * groupTip = new GroupTipItem();
    groupTip->setAttribute(Qt::WA_StyledBackground, true);
    BindGroupTipBackground(groupTip);

    QListWidgetItem *item = new QListWidgetItem;
    item->setSizeHint(groupTip->sizeHint());
    this->addItem(item);
    this->setItemWidget(item, groupTip);
    // 保持不可选中状态
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);

    // 添加新的朋友条目
    _add_friend_item = new ConUserItem();
    _add_friend_item->setObjectName("new_friend_item");
    _add_friend_item->addNewFriend();
    _add_friend_item->SetItemType(ListItemType::APPLY_FRIEND_ITEM);

    QListWidgetItem *add_item = new QListWidgetItem;
    add_item->setSizeHint(_add_friend_item->sizeHint());
    this->addItem(add_item);
    this->setItemWidget(add_item, _add_friend_item);

    auto * groupCon = new GroupTipItem();
    groupCon->SetGroupTip(tr("联系人"));

    groupCon->setAttribute(Qt::WA_StyledBackground, true);
    BindGroupTipBackground(groupCon);

    _groupitem = new QListWidgetItem;
    _groupitem->setSizeHint(groupCon->sizeHint());
    this->addItem(_groupitem);
    this->setItemWidget(_groupitem, groupCon);
    // 保持不可选中状态
    _groupitem->setFlags(_groupitem->flags() & ~Qt::ItemIsSelectable);

    auto con_list = UserMgr::GetInstance()->GetConListPerPage();
    for(auto& con_ele : con_list){
        auto* con_user_wid = new ConUserItem();
        con_user_wid->SetInfo(con_ele);
        QListWidgetItem* item = new QListWidgetItem;
        item->setSizeHint(con_user_wid->sizeHint());
        this->addItem(item);
        this->setItemWidget(item,con_user_wid);
        con_user_wid->SetItemType(ListItemType::CONTACT_USER_ITEM);
    }

    // 模拟联系人
    // for(int i = 0; i < 13; i++){
    //     int randomValue = QRandomGenerator::global()->bounded(100);
    //     int str_i = randomValue % str.size();
    //     int head_i = randomValue % head.size();
    //     int name_i = randomValue % name.size();

    //     auto *con_user_wid = new ConUserItem();
    //     con_user_wid->SetInfo(0, name[name_i], head[head_i]);
    //     QListWidgetItem *item = new QListWidgetItem;
    //     item->setSizeHint(con_user_wid->sizeHint());
    //     this->addItem(item);
    //     this->setItemWidget(item, con_user_wid);
    //     con_user_wid->SetItemType(ListItemType::CONTACT_USER_ITEM);
    // }
}

void ContactUserList::slot_item_clicked(QListWidgetItem *item)
{
    QWidget* wid = this->itemWidget(item);
    if(!wid){
        return ;
    }

    ListItemBase* it = qobject_cast<ListItemBase*>(wid);
    if(!it){
        return ;
    }

    auto itemType = it->GetItemType();
    if(itemType == ListItemType::INVALID_ITEM
        || itemType == ListItemType::GROUP_TIP_ITEM){
        return;
    }

    if(itemType == ListItemType::APPLY_FRIEND_ITEM){
        // 创建对话框，提示用户
        //跳转到好友申请界面
        emit sig_switch_apply_friend_page();
        return;
    }

    if(itemType == ListItemType::CONTACT_USER_ITEM){
        // 创建对话框，提示用户
        //跳转到好友信息界面
        auto con_item = qobject_cast<ConUserItem*>(it);
        auto user_info = con_item->GetInfo();
        emit sig_switch_friend_info_page(user_info);
        return;
    }
}

void ContactUserList::slot_add_auth_firend(std::shared_ptr<AuthInfo> auth_info)
{
    bool isFriend = UserMgr::GetInstance()->CheckFriendById(auth_info->_uid);
    if(isFriend || hasItemForUid(auth_info->_uid)){
        return;
    }

    // 在 groupitem 之后插入新项
    // int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    // int str_i = randomValue%str.size();
    // int head_i = randomValue%head.size();

    // // 测试使用随机头像
    // auth_info->_icon = head[head_i];

    auto *con_user_wid = new ConUserItem();
    con_user_wid->SetInfo(auth_info);
    QListWidgetItem *item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(con_user_wid->sizeHint());

    // 获取 groupitem 的索引
    int index = this->row(_groupitem);
    // 在 groupitem 之后插入新项
    this->insertItem(index + 1, item);

    this->setItemWidget(item, con_user_wid);

}

void ContactUserList::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp)
{
    bool isFriend = UserMgr::GetInstance()->CheckFriendById(auth_rsp->_uid);
    if(isFriend || hasItemForUid(auth_rsp->_uid)){
        return;
    }
    // 在 groupitem 之后插入新项
    // int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    // int str_i = randomValue%str.size();
    // int head_i = randomValue%head.size();

    auto *con_user_wid = new ConUserItem();
    // AuthRsp 重载携带备注（back），三参版会把备注丢掉
    con_user_wid->SetInfo(auth_rsp);

    QListWidgetItem *item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(con_user_wid->sizeHint());

    // 获取 groupitem 的索引
    int index = this->row(_groupitem);
    // 在 groupitem 之后插入新项
    this->insertItem(index + 1, item);

    this->setItemWidget(item, con_user_wid);
}


void ContactUserList::slot_remove_contact_user(int uid)
{
    if (uid == -1) {
        return;
    }
    for (int row = 0; row < count(); ++row) {
        QListWidgetItem* item = this->item(row);
        QWidget* widget = itemWidget(item);
        auto* conItem = qobject_cast<ConUserItem*>(widget);
        if (!conItem || !conItem->GetInfo() || conItem->GetInfo()->_uid != uid) {
            continue;
        }
        if (widget) {
            widget->deleteLater();
        }
        delete takeItem(row);
        return;
    }
}

bool ContactUserList::hasItemForUid(int uid) const
{
    for (int row = 0; row < count(); ++row) {
        auto* conItem = qobject_cast<ConUserItem*>(itemWidget(item(row)));
        if (conItem && conItem->GetInfo() && conItem->GetInfo()->_uid == uid) {
            return true;
        }
    }
    return false;
}

void ContactUserList::slot_friend_status(int uid, int status)
{
    for (int row = 0; row < count(); ++row) {
        auto* conItem = qobject_cast<ConUserItem*>(itemWidget(item(row)));
        if (!conItem || !conItem->GetInfo() || conItem->GetInfo()->_uid != uid) {
            continue;
        }
        conItem->SetStatus(status);
        return;
    }
}
