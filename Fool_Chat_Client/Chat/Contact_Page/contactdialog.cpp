#include "contactdialog.h"
#include "ui_contactdialog.h"
#include "../Chat_Comp/themedtext.h"
#include "../c_window.h"
#include "../Chat_Comp/conuseritem.h"
#include "../../src/core/Logger.h"
#include "../../src/core/tcpmgr.h"
#include "msgtip.h"
#include "ElaToolButton.h"
#include <QHBoxLayout>

ContactDialog::ContactDialog(QWidget *parent)
    : Page_Base(parent)
    , ui(new Ui::ContactDialog),_mode(ChatUIMode::ContactMode),_state(ChatUIMode::ContactMode),_b_loading(false),
    _last_widget(nullptr)
{
    setWindowTitle("联系人");
    // ElaScrollPage 自带内部布局，页面 UI 挂到滚动区内容上
    auto* content = new QWidget();
    this->setTitleVisible(false);
    ui->setupUi(content);
    // ElaText 颜色绑定主题（自带样式表不吃 palette 更新）
    ui->label->setTextStyle(ElaTextType::Body);
    BindTextToTheme(ui->label);
    addCentralWidget(content);
    // 标题行右侧加刷新按钮：重新拉取服务端好友/申请列表
    auto* refresh_btn = new ElaToolButton(this);
    refresh_btn->setIsTransparent(true);
    refresh_btn->setElaIcon(ElaIconType::ArrowsRotate);
    refresh_btn->setToolTip("刷新通讯录");
    refresh_btn->setFixedSize(36, 36);
    auto* header_row = new QHBoxLayout();
    header_row->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout_2->removeWidget(ui->label);
    header_row->addWidget(ui->label);
    header_row->addStretch();
    header_row->addWidget(refresh_btn);
    ui->verticalLayout_2->insertLayout(0, header_row);
    connect(refresh_btn, &ElaToolButton::clicked, this, [this]() {
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_FRIEND_LIST_REQ, QByteArray("{}"));
    });
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friend_list_refreshed,
            this, &ContactDialog::slot_friend_list_refreshed);
    ShowSearch(false);
    ui->search_list->SetSearchEdit(ui->search_edit);
    connect(ui->search_edit, &C_SearchEdit::textChanged, [=](const QString& text) {
        if (text.isEmpty()) {
            ShowSearch(false);
        }
    });
    connect(ui->search_edit,&C_SearchEdit::textChanged,this,&ContactDialog::slot_text_change);

    // 连接父窗口 添加联系人
    QWidget* parentWindow = this->window();
    C_Window* cWindow = qobject_cast<C_Window*>(parentWindow);

    connect(cWindow,&C_Window::SigContactApply,this,&ContactDialog::slot_add_apply);
    // 连接加载联系人
    connect(ui->con_user_list,&ContactUserList::sig_loading_contact_user,
            this,&ContactDialog::slot_loading_contact_user);
    // 连接点击用户切换用户信息
    connect(ui->con_user_list,&ContactUserList::sig_switch_friend_info_page,
            this,&ContactDialog::slot_friend_info_page);
    // 连接点击好友申请列表
    connect(ui->con_user_list,&ContactUserList::sig_switch_apply_friend_page,
            this,&ContactDialog::slot_switch_apply_friend_page);
    // 连接点击好友发送信息跳转
    connect(ui->friend_info_page,&FriendInfoPage::sig_jump_chat_item,
            this,&ContactDialog::slot_jump_chat_item_from_infopage);

    // 注册事件
    this->installEventFilter(this);
    qApp->installEventFilter(this);
}

ContactDialog::~ContactDialog()
{
    delete ui;
}

void ContactDialog::ShowSearch(bool bsearch)
{
    if(bsearch){
        ui->con_user_list->hide();
        ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }else if(_state == ChatUIMode::ChatMode){
        ui->con_user_list->hide();
        ui->search_list->hide();
        _mode = ChatUIMode::ChatMode;
    }else if(_state == ChatUIMode::ContactMode){
        ui->search_list->hide();
        ui->con_user_list->show();
        _mode = ChatUIMode::ContactMode;
    }
}

void ContactDialog::slot_text_change(const QString &str)
{
    if(!str.isEmpty()){
        ShowSearch(true);
    }
}

void ContactDialog::slot_add_apply(std::shared_ptr<AddFriendApply> apply)
{
    ui->friend_apply_page->AddNewApply(apply);
}

void ContactDialog::slot_loading_contact_user()
{
    if(_b_loading){
        return;
    }

    _b_loading = true;
    LoadingDlg *loadingDialog = new LoadingDlg(this);
    loadingDialog->setModal(true);
    loadingDialog->show();
    loadMoreConUser();
    // 加载完成后关闭对话框
    loadingDialog->deleteLater();

    _b_loading = false;
}

void ContactDialog::slot_friend_info_page(std::shared_ptr<UserInfo> user_info)
{
    _last_widget = ui->friend_info_page;
    _info_uid = user_info ? user_info->_uid : -1;
    ui->stackedWidget->setCurrentWidget(ui->friend_info_page);
    ui->friend_info_page->SetInfo(user_info);
}

void ContactDialog::slot_switch_apply_friend_page()
{
    _last_widget = ui->friend_apply_page;
    ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
}

void ContactDialog::slot_jump_chat_item_from_infopage(std::shared_ptr<UserInfo> user_info)
{
    if(!user_info){
        return;
    }
    emit sig_jump_chat_item(user_info);
    emit sig_switch_to_chat_page();
}

bool ContactDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        handleGlobalMousePress(mouseEvent);

    }
    return QWidget::eventFilter(watched,event);
}

void ContactDialog::handleGlobalMousePress(QMouseEvent* event)
{
    if (_mode != ChatUIMode::SearchMode) {
        return;
    }

    QWidget* clickedWidget = QApplication::widgetAt(event->globalPos());

    // 如果点击的是当前对话框内的元素，不处理
    if (this->isAncestorOf(clickedWidget)) {
        // 检查是否是搜索相关的元素
        bool isSearchRelated = false;

        if (ui->search_edit->isAncestorOf(clickedWidget) ||
            clickedWidget == ui->search_edit) {
            isSearchRelated = true;
        }

        if (ui->search_list->isAncestorOf(clickedWidget) ||
            clickedWidget == ui->search_list) {
            isSearchRelated = true;
        }

        // 如果不是搜索相关元素，则关闭搜索
        if (!isSearchRelated) {
            ui->search_edit->clear();
            ShowSearch(false);
        }
    }
    // 如果点击的是对话框外的元素，关闭搜索
    else {
        ui->search_edit->clear();
        ShowSearch(false);
    }
}

void ContactDialog::loadMoreConUser()
{
    auto friend_list = UserMgr::GetInstance()->GetConListPerPage();
    if(friend_list.empty() == false){
        for(auto& friend_ele : friend_list){
            auto* chat_user_wid = new ConUserItem();
            chat_user_wid->SetInfo(friend_ele);
            QListWidgetItem* item = new QListWidgetItem;
            item->setSizeHint(chat_user_wid->sizeHint());
            ui->con_user_list->addItem(item);
            ui->con_user_list->setItemWidget(item,chat_user_wid);
        }
        // 更新已加载条目
        UserMgr::GetInstance()->UpdateChatLoadedCount();
    }
}

void ContactDialog::ShowFriendInfo(int uid)
{
    auto info = UserMgr::GetInstance()->GetFriendById(uid);
    if (!info) {
        return;
    }
    ui->friend_info_page->SetInfo(std::make_shared<UserInfo>(info));
    ui->stackedWidget->setCurrentWidget(ui->friend_info_page);
    _info_uid = uid;
}

void ContactDialog::slot_remove_contact_user(int uid)
{
    // 详情页正显示被删好友 → 切回默认的申请页
    if (ui->stackedWidget->currentWidget() == ui->friend_info_page && _info_uid == uid) {
        ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
        _info_uid = -1;
    }
    ui->con_user_list->slot_remove_contact_user(uid);
    // 服务端已删除双方 friend_apply，申请列表条目同步移除
    ui->friend_apply_page->RemoveApply(uid);
}

void ContactDialog::slot_friend_list_refreshed(bool ok)
{
    if (!ok) {
        ADDMSG(ElaMessageBarType::Top, "刷新好友列表失败", this, 0, 2000);
        return;
    }
    auto* mgr = UserMgr::GetInstance().get();

    // 1) 移除服务端已不存在的联系人条目（AI/分组头/新的朋友入口不动）
    QList<int> stale;
    for (int row = 0; row < ui->con_user_list->count(); ++row) {
        auto* conItem = qobject_cast<ConUserItem*>(
            ui->con_user_list->itemWidget(ui->con_user_list->item(row)));
        if (conItem == nullptr || conItem->GetInfo() == nullptr) {
            continue;
        }
        int uid = conItem->GetInfo()->_uid;
        // uid<=0 是功能入口（AI=-1、新的朋友=0）不是好友，不能按"服务端已删"清掉
        if (uid > 0 && mgr->GetFriendById(uid) == nullptr) {
            stale.append(uid);
        }
    }
    for (int uid : stale) {
        // 详情页正显示被移除的好友则切回申请页
        if (ui->stackedWidget->currentWidget() == ui->friend_info_page && _info_uid == uid) {
            ui->stackedWidget->setCurrentWidget(ui->friend_apply_page);
            _info_uid = -1;
        }
        ui->con_user_list->slot_remove_contact_user(uid);
    }

    // 2) 同步仍存在条目资料（SetInfo 内含名字/头像/状态点重绘）
    for (int row = 0; row < ui->con_user_list->count(); ++row) {
        auto* conItem = qobject_cast<ConUserItem*>(
            ui->con_user_list->itemWidget(ui->con_user_list->item(row)));
        if (conItem == nullptr || conItem->GetInfo() == nullptr) {
            continue;
        }
        auto fi = mgr->GetFriendById(conItem->GetInfo()->_uid);
        if (fi != nullptr) {
            conItem->SetInfo(fi);
        }
    }

    // 3) 补新增好友（hasItemForUid 去重，追加到列表尾部联系人区）
    for (auto& fi : mgr->GetAllFriends()) {
        if (ui->con_user_list->hasItemForUid(fi->_uid)) {
            continue;
        }
        auto* wid = new ConUserItem();
        wid->SetInfo(fi);
        wid->SetItemType(ListItemType::CONTACT_USER_ITEM);
        auto* item = new QListWidgetItem;
        item->setSizeHint(wid->sizeHint());
        ui->con_user_list->addItem(item);
        ui->con_user_list->setItemWidget(item, wid);
    }

    // 4) 申请列表按最新数据整页重建
    ui->friend_apply_page->ReloadApplyList();
}
