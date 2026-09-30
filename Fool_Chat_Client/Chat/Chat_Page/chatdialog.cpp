#include "chatdialog.h"
#include "ui_chatdialog.h"
#include <QRandomGenerator>
#include <QUuid>
#include "Chat_Comp/chatuseritem.h"
#include "Chat_Comp/loadingdlg.h"
#include <QMouseEvent>
#include "tcpmgr.h"
#include "usermgr.h"
#include "msgtip.h"
#include "ElaToolButton.h"
#include "Logger.h"

ChatDialog::ChatDialog(QWidget *parent) :
    Page_Base(parent),ui(new Ui::ChatDialog),_b_loading(false),
    _mode(ChatUIMode::ChatMode),_state(ChatUIMode::ChatMode),
    _cur_chat_uid(0)
{
    // ElaScrollPage 自带内部布局，页面 UI 挂到滚动区内容上
    auto* content = new QWidget();
    ui->setupUi(content);
    this->setTitleVisible(false);
    addCentralWidget(content);
    ui->search_edit->setFocusPolicy(Qt::ClickFocus);
    ui->add_btn->setIsTransparent(false);
    ui->add_btn->setElaIcon(ElaIconType::UserPlus);
    ui->add_btn->setToolTip("添加联系人");
    // 刷新：重新拉取服务端好友/申请列表（回包信号统一重建两页）
    auto* refresh_btn = new ElaToolButton(ui->search_wid);
    refresh_btn->setIsTransparent(true);
    refresh_btn->setElaIcon(ElaIconType::ArrowsRotate);
    refresh_btn->setToolTip("刷新会话列表");
    refresh_btn->setFixedSize(36, 36);
    ui->horizontalLayout_2->addWidget(refresh_btn);
    connect(refresh_btn, &ElaToolButton::clicked, this, [this]() {
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_FRIEND_LIST_REQ, QByteArray("{}"));
    });
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friend_list_refreshed,
            this, &ChatDialog::slot_friend_list_refreshed);
    ui->search_list->SetSearchEdit(ui->search_edit);
    ui->stackedWidget->setCurrentWidget(ui->chat_page);

    UserMgr::GetInstance()->RegisterAiFriend();
    addAiChat();
    addChatUserList();
    // 打开聊天页默认选中 AI 助手：列表高亮第一行 + 聊天区加载 AI 会话
    SetSelectChatItem(0);
    SetSelectChatPage(0);

    ShowSearch(false);
    connect(ui->search_edit, &C_SearchEdit::textChanged, [=](const QString& text) {
        if (text.isEmpty()) {
            ShowSearch(false);
        }
    });
    connect(ui->chat_user_list, &ChatUserList::sig_loading_chat_user, this, &ChatDialog::slot_loading_chat_user);
    connect(ui->search_edit,&C_SearchEdit::textChanged,this,&ChatDialog::slot_text_change);

    this->installEventFilter(this);
    qApp->installEventFilter(this);

    // 连接认证添加好友信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_add_auth_friend, this, &ChatDialog::slot_add_auth_friend);

    // 连接自己认证回复信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_auth_rsp, this,
            &ChatDialog::slot_auth_rsp);

    // 添加好友时是已添加好友，触发跳转到好友聊天界面
    connect(ui->search_list,&SearchList::sig_jump_chat_item,this,&ChatDialog::slot_jump_chat_item);

    // 连接聊天列表点击
    connect(ui->chat_user_list,&QListWidget::itemClicked,this,&ChatDialog::slot_item_clicked);

    // 连接消息添加
    connect(ui->chat_page,&ChatWid::sig_append_send_chat_msg,this,&ChatDialog::slot_append_send_chat_msg);
    connect(ui->chat_page, &ChatWid::sig_ai_summary_reply, this, &ChatDialog::slot_ai_summary_reply);

    // 连接对端消息通知
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_text_chat_msg,this,&ChatDialog::slot_text_chat_msg);

    // 好友上下线：同步会话条目内存快照 + 当前会话头部状态点
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_friend_status, this, [this](int uid, int status) {
        auto iter = _chat_items_added.find(uid);
        if (iter != _chat_items_added.end()) {
            if (auto* wid = qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(iter.value()))) {
                auto info = wid->GetUserInfo();
                if (info) {
                    info->_status = status;
                }
            }
        }
        if (_cur_chat_uid == uid) {
            ui->chat_page->SetStatus(status == 1);
        }
    });

    // 气泡头像点击：转发给主窗口跳好友资料页
    connect(ui->chat_page, &ChatWid::sig_show_profile, this, &ChatDialog::sig_show_friend_profile);

    // 备注修改成功：同步条目快照的 _back 后刷新显示名
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_back_updated, this,
            [this](int uid, const QString& back) {
        auto iter = _chat_items_added.find(uid);
        if (iter != _chat_items_added.end()) {
            if (auto* wid = qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(iter.value()))) {
                auto info = wid->GetUserInfo();
                if (info) {
                    info->_back = back;
                    wid->SetInfo(info);
                }
            }
        }
        // 当前会话的头部标题同步
        if (_cur_chat_uid == uid) {
            auto info = ui->chat_page->GetUserInfo();
            if (info) {
                info->_back = back;
                ui->chat_page->SetUserInfo(info);
            }
        }
    });
}

ChatDialog::~ChatDialog()
{
    delete ui;
}

void ChatDialog::ShowSearch(bool bsearch)
{
    if(bsearch){
        ui->chat_user_list->hide();
        ui->con_user_list->hide();
        ui->search_list->show();
        _mode = ChatUIMode::SearchMode;
    }else if(_state == ChatUIMode::ChatMode){
        ui->chat_user_list->show();
        ui->con_user_list->hide();
        ui->search_list->hide();
        _mode = ChatUIMode::ChatMode;
    }else if(_state == ChatUIMode::ContactMode){
        ui->chat_user_list->hide();
        ui->search_list->hide();
        ui->con_user_list->show();
        _mode = ChatUIMode::ContactMode;
    }
}

bool ChatDialog::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress){
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        handleGlobalMousePress(mouseEvent);

    }
    return QWidget::eventFilter(watched,event);
}

void ChatDialog::addChatUserList()
{
    //先按照好友列表加载聊天记录，等以后客户端实现聊天记录数据库之后再按照最后信息排序
    auto friend_list = UserMgr::GetInstance()->GetChatListPerPage();

    if (friend_list.empty() == false) {
        for(auto & friend_ele : friend_list){
            auto find_iter = _chat_items_added.find(friend_ele->_uid);
            if(find_iter != _chat_items_added.end()){
                continue;
            }
            auto *chat_user_wid = new ChatUseritem();
            auto user_info = std::make_shared<UserInfo>(friend_ele);
            chat_user_wid->SetInfo(user_info);
            QListWidgetItem *item = new QListWidgetItem;
            // qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
            item->setSizeHint(chat_user_wid->sizeHint());
            placeChatItem(friend_ele->_uid, item);
            ui->chat_user_list->setItemWidget(item, chat_user_wid);
            _chat_items_added.insert(friend_ele->_uid, item);
        }

        //更新已加载条目
        UserMgr::GetInstance()->UpdateChatLoadedCount();
    }

    // 模拟添加数据
    // for(int i = 0; i < 13; i++){
    //     int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    //     int str_i = randomValue%str.size();
    //     int head_i = randomValue%head.size();
    //     int name_i = randomValue%name.size();

    //     auto *chat_user_wid = new ChatUseritem();
    //     auto user_info = std::make_shared<UserInfo>(0,name[name_i],
    //                                                 name[name_i],head[head_i],0,str[str_i]);
    //     chat_user_wid->SetInfo(user_info);
    //     QListWidgetItem *item = new QListWidgetItem;
    //     //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    //     item->setSizeHint(chat_user_wid->sizeHint());
    //     ui->chat_user_list->addItem(item);
    //     ui->chat_user_list->setItemWidget(item, chat_user_wid);
    // }
}

void ChatDialog::addAiChat()
{
    auto *chat_user_wid = new ChatUseritem(ui->chat_user_list);
    auto user_info = std::make_shared<UserInfo>(-1,"AI助手",
                                                "AI助手",":/icons/image.png",0,"你好");
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem *item = new QListWidgetItem;
    item->setSizeHint(chat_user_wid->sizeHint());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    chat_user_wid->setStyleSheet(  "ChatUseritem {"
                                 "   background-color: rgba(160, 160, 160, 0.1);"
                                 "}");
    ui->chat_user_list->addItem(item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_items_added.insert(-1, item);
}

void ChatDialog::handleGlobalMousePress(QMouseEvent* event)
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

void ChatDialog::slot_loading_chat_user()
{
    if(_b_loading){
        return;
    }
    _b_loading = true;
    LoadingDlg *load = new LoadingDlg(this);
    load->setModal(true);
    load->show();
    addChatUserList();
    load->deleteLater();
    _b_loading = false;
}

void ChatDialog::slot_text_change(const QString &str)
{
    if(!str.isEmpty()){
        ShowSearch(true);
    }
}

void ChatDialog::slot_add_auth_friend(std::shared_ptr<AuthInfo> auth_info)
{
    //判断如果已经是好友则跳过
    auto bfriend = UserMgr::GetInstance()->CheckFriendById(auth_info->_uid);
    if(bfriend || _chat_items_added.contains(auth_info->_uid)){
        return;
    }

    UserMgr::GetInstance()->AddFriend(auth_info);

    // int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    // int str_i = randomValue % str.size();
    // int head_i = randomValue % head.size();
    // int name_i = randomValue % name.size();
    // auth_info->_icon = head[head_i];
    auto* chat_user_wid = new ChatUseritem();
    auto user_info = std::make_shared<UserInfo>(auth_info);
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem* item = new QListWidgetItem;

    item->setSizeHint(chat_user_wid->sizeHint());
    // 置顶之后、普通区开头插入（AI 恒在 index0，置顶紧随其后）
    ui->chat_user_list->insertItem(firstNormalRow(), item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_items_added.insert(auth_info->_uid, item);
}

void ChatDialog::slot_auth_rsp(std::shared_ptr<AuthRsp> auth_rsp)
{
    //判断如果已经是好友则跳过
    auto bfriend = UserMgr::GetInstance()->CheckFriendById(auth_rsp->_uid);
    if(bfriend || _chat_items_added.contains(auth_rsp->_uid)){
        return;
    }

    UserMgr::GetInstance()->AddFriend(auth_rsp);
    // int randomValue = QRandomGenerator::global()->bounded(100); // 生成0到99之间的随机整数
    // int str_i = randomValue % str.size();
    // int head_i = randomValue % head.size();
    // int name_i = randomValue % name.size();
    // auth_rsp->_icon = head[head_i];

    auto* chat_user_wid = new ChatUseritem();
    auto user_info = std::make_shared<UserInfo>(auth_rsp);
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    // 置顶之后、普通区开头插入（AI 恒在 index0，置顶紧随其后）
    ui->chat_user_list->insertItem(firstNormalRow(), item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_items_added.insert(auth_rsp->_uid, item);
}

void ChatDialog::slot_jump_chat_item(std::shared_ptr<SearchInfo> si)
{
    if(!si){
        return;
    }
    auto find_iter = _chat_items_added.find(si->_uid);
    if(find_iter != _chat_items_added.end()){
        ui->chat_user_list->scrollToItem(find_iter.value());
        SetSelectChatItem(si->_uid);
        //更新聊天界面信息
        SetSelectChatPage(si->_uid);
        return;
    }

    //如果没找到，则创建新的插入listwidget
    auto* chat_user_wid = new ChatUseritem();
    auto user_info = std::make_shared<UserInfo>(si);
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem* item = new QListWidgetItem;
    item->setSizeHint(chat_user_wid->sizeHint());
    // 置顶之后、普通区开头插入（AI 恒在 index0，置顶紧随其后）
    ui->chat_user_list->insertItem(firstNormalRow(), item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);

    _chat_items_added.insert(si->_uid, item);

    SetSelectChatItem(si->_uid);
    //更新聊天界面信息
    SetSelectChatPage(si->_uid);
}

void ChatDialog::SetSelectChatPage(int uid)
{
    if( ui->chat_user_list->count() <= 0){
        return;
    }

    if (uid == 0) {
        auto item = ui->chat_user_list->item(0);
        //转为widget
        QWidget* widget = ui->chat_user_list->itemWidget(item);
        if (!widget) {
            return;
        }

        auto con_item = qobject_cast<ChatUseritem*>(widget);
        if (!con_item) {
            return;
        }

        //设置信息
        auto user_info = con_item->GetUserInfo();
        ui->chat_page->SetUserInfo(user_info);
        clearUnread(user_info->_uid);
        return;
    }

    auto find_iter = _chat_items_added.find(uid);
    if(find_iter == _chat_items_added.end()){
        return;
    }

    //转为widget
    QWidget *widget = ui->chat_user_list->itemWidget(find_iter.value());
    if(!widget){
        return;
    }

    //判断转化为自定义的widget
    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        LOG_ERROR("chat list item cast to ListItemBase failed");
        return;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == CHAT_USER_ITEM){
        auto con_item = qobject_cast<ChatUseritem*>(customItem);
        if(!con_item){
            return;
        }

        //设置信息
        auto user_info = con_item->GetUserInfo();
        ui->chat_page->SetUserInfo(user_info);
        clearUnread(user_info->_uid);

        return;
    }
}

void ChatDialog::slot_jump_chat_item_from_user_info(std::shared_ptr<UserInfo> user_info)
{
    if(!user_info){
        return;
    }
    auto find_iter = _chat_items_added.find(user_info->_uid);
    if(find_iter != _chat_items_added.end()){
        ui->chat_user_list->scrollToItem(find_iter.value());
        SetSelectChatItem(user_info->_uid);
        //更新聊天界面信息
        SetSelectChatPage(user_info->_uid);
        return;
    }

    //如果没找到，则创建新的插入listwidget
    auto* chat_user_wid = new ChatUseritem();
    chat_user_wid->SetInfo(user_info);
    QListWidgetItem* item = new QListWidgetItem;
    item->setSizeHint(chat_user_wid->sizeHint());
    // 置顶之后、普通区开头插入（AI 恒在 index0，置顶紧随其后）
    ui->chat_user_list->insertItem(firstNormalRow(), item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);

    _chat_items_added.insert(user_info->_uid, item);

    SetSelectChatItem(user_info->_uid);
    //更新聊天界面信息
    SetSelectChatPage(user_info->_uid);
}

void ChatDialog::slot_append_send_chat_msg(std::shared_ptr<TextChatData> msg)
{
    if(_cur_chat_uid==0){
        return;
    }

    auto find_iter = _chat_items_added.find(_cur_chat_uid);
    if (find_iter == _chat_items_added.end()) {
        return;
    }

    //转为widget
    QWidget* widget = ui->chat_user_list->itemWidget(find_iter.value());
    if (!widget) {
        return;
    }

    //判断转化为自定义的widget
    // 对自定义widget进行操作， 将item 转化为基类ListItemBase
    ListItemBase* customItem = qobject_cast<ListItemBase*>(widget);
    if (!customItem) {
        LOG_ERROR("chat list item cast to ListItemBase failed");
        return;
    }

    auto itemType = customItem->GetItemType();
    if (itemType == CHAT_USER_ITEM) {
        auto con_item = qobject_cast<ChatUseritem*>(customItem);
        if (!con_item) {
            return;
        }

        //设置信息
        auto user_info = con_item->GetUserInfo();
        user_info->_chat_msgs.push_back(msg);
        std::vector<std::shared_ptr<TextChatData>> msg_vec;
        msg_vec.push_back(msg);
        con_item->updateLastMsg(msg_vec);
        UserMgr::GetInstance()->AppendFriendChatMsg(_cur_chat_uid,msg_vec);
        return;
    }
}

void ChatDialog::slot_ai_summary_reply(int targetUid, std::shared_ptr<TextChatData> msg)
{
    // 历史与会话行预览按目标好友记账（与当前看哪个会话无关）
    UserMgr::GetInstance()->AppendFriendChatMsg(targetUid, {msg});
    auto iter = _chat_items_added.find(targetUid);
    if (iter != _chat_items_added.end()) {
        if (auto* wid = qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(iter.value()))) {
            if (auto info = wid->GetUserInfo()) {
                info->_chat_msgs.push_back(msg);
                wid->updateLastMsg({msg});
            }
        }
    }
    // 气泡只在目标会话可见时插入，判定与普通消息走同一真源 _cur_chat_uid
    if (_cur_chat_uid == targetUid) {
        ui->chat_page->AppendChatMsg(msg, true);
    }
}

void ChatDialog::slot_text_chat_msg(std::shared_ptr<TextChatMsg> msg)
{
    // 非当前打开的会话（且不是自己回显）才计入未读红点
    const int self_uid = UserMgr::GetInstance()->GetUid();
    if (msg->_from_uid != _cur_chat_uid && msg->_from_uid != self_uid) {
        markUnread(msg->_from_uid);
    }

    auto find_iter = _chat_items_added.find(msg->_from_uid);
    if(find_iter != _chat_items_added.end()){
        QWidget *widget = ui->chat_user_list->itemWidget(find_iter.value());
        auto chat_wid = qobject_cast<ChatUseritem*>(widget);
        if(!chat_wid){
            return;
        }
        chat_wid->updateLastMsg(msg->_chat_msgs);
        //更新当前聊天页面记录
        UpdateChatMsg(msg->_chat_msgs);
        UserMgr::GetInstance()->AppendFriendChatMsg(msg->_from_uid,msg->_chat_msgs);
        return;
    }

    //如果没找到，则创建新的插入listwidget

    //查询好友信息
    auto fi_ptr = UserMgr::GetInstance()->GetFriendById(msg->_from_uid);
    if (!fi_ptr) {
        // 非好友消息无法构造会话条目（SetInfo 内部解引用），红点已记账，等好友列表刷新后补建
        return;
    }

    auto* chat_user_wid = new ChatUseritem();
    chat_user_wid->SetInfo(fi_ptr);
    QListWidgetItem* item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(chat_user_wid->sizeHint());
    chat_user_wid->updateLastMsg(msg->_chat_msgs);
    UserMgr::GetInstance()->AppendFriendChatMsg(msg->_from_uid,msg->_chat_msgs);
    // 置顶之后、普通区开头插入（AI 恒在 index0，置顶紧随其后）
    ui->chat_user_list->insertItem(firstNormalRow(), item);
    ui->chat_user_list->setItemWidget(item, chat_user_wid);
    _chat_items_added.insert(msg->_from_uid, item);
    // 新建条目补上此前累计的未读红点
    chat_user_wid->SetUnread(_unread_map.value(msg->_from_uid, 0));
}

void ChatDialog::markUnread(int uid)
{
    _unread_map[uid] = _unread_map.value(uid, 0) + 1;
    auto iter = _chat_items_added.find(uid);
    if (iter != _chat_items_added.end()) {
        if (auto* wid = qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(iter.value()))) {
            wid->SetUnread(_unread_map[uid]);
        }
    }
    refreshUnreadBadge();
}

void ChatDialog::clearUnread(int uid)
{
    if (!_unread_map.contains(uid)) {
        return;
    }
    _unread_map.remove(uid);
    auto iter = _chat_items_added.find(uid);
    if (iter != _chat_items_added.end()) {
        if (auto* wid = qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(iter.value()))) {
            wid->SetUnread(0);
        }
    }
    refreshUnreadBadge();
}

void ChatDialog::refreshUnreadBadge()
{
    int total = 0;
    for (auto it = _unread_map.constBegin(); it != _unread_map.constEnd(); ++it) {
        total += it.value();
    }
    emit sig_chat_unread(total);
}

void ChatDialog::slot_remove_chat_user(int uid)
{
    auto iter = _chat_items_added.find(uid);
    if (iter == _chat_items_added.end()) {
        return;
    }
    QListWidgetItem* item = iter.value();
    QWidget* widget = ui->chat_user_list->itemWidget(item);
    const int row = ui->chat_user_list->row(item);
    if (widget) {
        widget->deleteLater();
    }
    delete ui->chat_user_list->takeItem(row);
    _chat_items_added.remove(uid);

    if (_cur_chat_uid == uid) {
        // 当前打开的就是被删好友 → 切回第一行（AI 会话）
        SetSelectChatItem(0);
        SetSelectChatPage(0);
    }
}

int ChatDialog::firstNormalRow() const
{
    // index0 固定为 AI；其后连续置顶条目结束的位置即普通区起点
    int row = 1;
    for (; row < ui->chat_user_list->count(); ++row) {
        auto* wid = qobject_cast<ChatUseritem*>(
            ui->chat_user_list->itemWidget(ui->chat_user_list->item(row)));
        if (!wid || !wid->GetUserInfo()) {
            break;
        }
        if (!UserMgr::GetInstance()->IsPinned(wid->GetUserInfo()->_uid)) {
            break;
        }
    }
    return row;
}

void ChatDialog::placeChatItem(int uid, QListWidgetItem* item)
{
    if (UserMgr::GetInstance()->IsPinned(uid)) {
        ui->chat_user_list->insertItem(firstNormalRow(), item);
    } else {
        ui->chat_user_list->addItem(item);
    }
}

void ChatDialog::togglePin(int uid)
{
    auto iter = _chat_items_added.find(uid);
    if (iter == _chat_items_added.end()) {
        return;
    }
    QListWidgetItem* oldItem = iter.value();
    const int oldRow = ui->chat_user_list->row(oldItem);
    if (oldRow < 0) {
        return;
    }
    QWidget* oldWidget = ui->chat_user_list->itemWidget(oldItem);
    auto* chatWid = qobject_cast<ChatUseritem*>(oldWidget);
    auto info = chatWid ? chatWid->GetUserInfo() : nullptr;
    if (!info) {
        return;
    }

    const bool nowPinned = !UserMgr::GetInstance()->IsPinned(uid);
    UserMgr::GetInstance()->SetPinned(uid, nowPinned);

    // 摘除旧条目（widget 先挂 deleteLater，takeItem 若连带销毁也能安全收敛）
    if (oldWidget) {
        oldWidget->deleteLater();
    }
    delete ui->chat_user_list->takeItem(oldRow);

    // 重建条目放到目标位置（复用同一份 UserInfo，聊天历史不丢）
    auto* newWid = new ChatUseritem();
    newWid->SetInfo(info);
    auto* newItem = new QListWidgetItem;
    newItem->setSizeHint(newWid->sizeHint());
    newItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    if (nowPinned) {
        ui->chat_user_list->insertItem(firstNormalRow(), newItem);
    } else {
        ui->chat_user_list->addItem(newItem);
    }
    ui->chat_user_list->setItemWidget(newItem, newWid);
    _chat_items_added[uid] = newItem;

    if (_cur_chat_uid == uid) {
        ui->chat_user_list->setCurrentItem(newItem);
    }
}

void ChatDialog::clearChatHistory(int uid)
{
    UserMgr::GetInstance()->ClearFriendChatMsg(uid);

    auto iter = _chat_items_added.find(uid);
    if (iter == _chat_items_added.end()) {
        return;
    }
    auto* chatWid = qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(iter.value()));
    if (!chatWid) {
        return;
    }
    auto info = chatWid->GetUserInfo();
    if (!info) {
        return;
    }
    info->_chat_msgs.clear();
    info->_last_msg.clear();
    chatWid->SetInfo(info); // 刷新会话行：最后一条消息置空

    if (_cur_chat_uid == uid) {
        // 当前打开的就是该好友：重放空历史 → 清掉聊天窗气泡
        ui->chat_page->SetUserInfo(info);
    }
}

void ChatDialog::UpdateChatMsg(std::vector<std::shared_ptr<TextChatData> > msgdata)
{
    for(auto & msg : msgdata){
        if(msg->_from_uid != _cur_chat_uid){
            break;
        }

        ui->chat_page->AppendChatMsg(msg);
    }
}


void ChatDialog::loadMoreChatUser()
{
    auto friend_list = UserMgr::GetInstance()->GetChatListPerPage();
    if(friend_list.empty() == false){
        for(auto& friend_ele : friend_list){
            auto find_iter = _chat_items_added.find(friend_ele->_uid);
            if (find_iter != _chat_items_added.end()){
                continue;
            }
            auto* chat_user_wid = new ChatUseritem();
            auto user_info = std::make_shared<UserInfo>(friend_ele);
            chat_user_wid->SetInfo(user_info);
            QListWidgetItem* item = new QListWidgetItem();
            item->setSizeHint(chat_user_wid->sizeHint());
            placeChatItem(friend_ele->_uid, item);
            ui->chat_user_list->setItemWidget(item,chat_user_wid);
            _chat_items_added.insert(friend_ele->_uid,item);
            // 分页补进的条目若已有未读，恢复红点
            chat_user_wid->SetUnread(_unread_map.value(friend_ele->_uid, 0));
        }

        // 更新已加载条目
        UserMgr::GetInstance()->UpdateChatLoadedCount();
    }
}

void ChatDialog::AppendLocalCallMsg(int peerUid, bool fromSelf, const QString& text)
{
    const int selfUid = UserMgr::GetInstance()->GetUid();
    auto data = std::make_shared<TextChatData>(
        QUuid::createUuid().toString(QUuid::WithoutBraces), text,
        fromSelf ? selfUid : peerUid, fromSelf ? peerUid : selfUid);
    std::vector<std::shared_ptr<TextChatData>> msgs{data};

    // 1. 内存模型（切换会话/重开页面后仍在）
    UserMgr::GetInstance()->AppendFriendChatMsg(peerUid, msgs);

    // 2. 会话列表条目：预览文案 + 快照
    auto find_iter = _chat_items_added.find(peerUid);
    if (find_iter != _chat_items_added.end()) {
        if (auto* itemWid =
                qobject_cast<ChatUseritem*>(ui->chat_user_list->itemWidget(find_iter.value()))) {
            itemWid->updateLastMsg(msgs);
        }
    }

    // 3. 当前正打开的聊天页直接插气泡（AppendChatMsg 按 from_uid 分左右）
    if (_cur_chat_uid == peerUid) {
        ui->chat_page->AppendChatMsg(data);
    }
}

void ChatDialog::showFriendProfile(int uid)
{
    emit sig_show_friend_profile(uid);
}

void ChatDialog::openFirstUnread()
{
    if (_unread_map.isEmpty()) {
        return;
    }
    const int uid = _unread_map.firstKey();
    SetSelectChatItem(uid);
    SetSelectChatPage(uid);
}

void ChatDialog::SetSelectChatItem(int uid)
{
    if(ui->chat_user_list->count() <= 0){
        return;
    }

    if(uid == 0){
        ui->chat_user_list->setCurrentRow(0);
        QListWidgetItem *firstItem = ui->chat_user_list->item(0);
        if(!firstItem){
            return;
        }

        //转为widget
        QWidget *widget = ui->chat_user_list->itemWidget(firstItem);
        if(!widget){
            return;
        }

        auto con_item = qobject_cast<ChatUseritem*>(widget);
        if(!con_item){
            return;
        }

        _cur_chat_uid = con_item->GetUserInfo()->_uid;

        return;
    }

    auto find_iter = _chat_items_added.find(uid);
    if(find_iter == _chat_items_added.end()){
        ui->chat_user_list->setCurrentRow(0);
        return;
    }

    ui->chat_user_list->setCurrentItem(find_iter.value());

    _cur_chat_uid = uid;
}

void ChatDialog::slot_item_clicked(QListWidgetItem *item)
{
    QWidget* wid = ui->chat_user_list->itemWidget(item);
    if(!wid){
        return;
    }

    ListItemBase* it = qobject_cast<ListItemBase*>(wid);
    if(!it){
        return;
    }

    auto itemType = it->GetItemType();
    if(itemType == ListItemType::INVALID_ITEM || itemType == ListItemType::GROUP_TIP_ITEM){
        return;
    }

    if(itemType == ListItemType::CHAT_USER_ITEM){
        auto chat_wid = qobject_cast<ChatUseritem*>(it);
        if(!chat_wid){
            return;
        }

        auto user_info = chat_wid->GetUserInfo();
        if(!user_info){
            return;
        }

        // 重复点击同一会话：页面本来就是它，只清未读，
        // 避免每次点击都全删气泡再从历史全量重放
        if (_cur_chat_uid == user_info->_uid) {
            clearUnread(user_info->_uid);
            return;
        }

        ui->chat_page->SetUserInfo(user_info);
        _cur_chat_uid = user_info->_uid;
        clearUnread(user_info->_uid);
        return;
    }


}


void ChatDialog::slot_friend_list_refreshed(bool ok)
{
    if (!ok) {
        ADDMSG(ElaMessageBarType::Top, "刷新好友列表失败", this, 0, 2000);
        return;
    }
    auto* mgr = UserMgr::GetInstance().get();

    // 1) 移除服务端已不存在的好友条目（AI 的 -1 永远保留）
    const auto stale_keys = _chat_items_added.keys();
    for (int uid : stale_keys) {
        if (uid != -1 && mgr->GetFriendById(uid) == nullptr) {
            clearUnread(uid);
            slot_remove_chat_user(uid);
        }
    }

    // 2) 同步仍存在条目的最新资料（备注/昵称/头像/状态）
    for (int uid : _chat_items_added.keys()) {
        auto fi = mgr->GetFriendById(uid);
        if (fi == nullptr) {
            continue;
        }
        auto* wid = qobject_cast<ChatUseritem*>(
            ui->chat_user_list->itemWidget(_chat_items_added[uid]));
        if (wid == nullptr || wid->GetUserInfo() == nullptr) {
            continue;
        }
        auto info = wid->GetUserInfo();
        info->_name = fi->_name;
        info->_nick = fi->_nick;
        info->_icon = fi->_icon;
        info->_sex = fi->_sex;
        info->_back = fi->_back;
        info->_status = fi->_status;
        wid->SetInfo(info);
    }

    // 3) 分页游标续拉新增好友（addChatUserList 内部按 _chat_items_added 去重）
    while (!mgr->IsLoadChatFin()) {
        addChatUserList();
    }

    // 4) 当前打开会话的头部同步（改备注/换头像后立即生效）
    if (_cur_chat_uid > 0 && _chat_items_added.contains(_cur_chat_uid)) {
        SetSelectChatPage(_cur_chat_uid);
    }
}
