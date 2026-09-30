#include "searchlist.h"
#include "tcpmgr.h"
#include "c_searchedit.h"
#include "adduseritem.h"
#include "msgtip.h"
#include "findsuccessdlg.h"
#include <QJsonDocument>
#include "usermgr.h"

SearchList::SearchList(QWidget *parent):QListWidget(parent),_find_dlg(nullptr), _search_edit(nullptr),_loadingDialog(nullptr), _send_pending(false)
{
    Q_UNUSED(parent);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);
    //连接点击的信号和槽
    connect(this, &QListWidget::itemClicked, this, &SearchList::slot_item_clicked);
    //添加条目
    addTipItem();
    //连接搜索条目
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_user_search, this, &SearchList::slot_user_search);
}

void SearchList::CloseFindDlg()
{
    if(_find_dlg){
        _find_dlg->hide();          // 先隐藏
        _find_dlg->deleteLater();   // 安全销毁（重点！）
        _find_dlg = nullptr;        // 置空防止野指针
    }
}

void SearchList::SetSearchEdit(QWidget *edit)
{
    _search_edit = edit;
}

bool SearchList::eventFilter(QObject *watched, QEvent *event)
{
    // 检查事件是否是鼠标悬浮进入或离开
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

        return true; // 停止事件传递
    }

    return QListWidget::eventFilter(watched, event);
}

void SearchList::waitPending(bool pending)
{
    if(pending){
        if (_loadingDialog) {
            return;
        }
        _loadingDialog = new LoadingDlg(this);
        _loadingDialog->setModal(true);
        _loadingDialog->show();
        _send_pending = true;
    } else {
        if(_loadingDialog != nullptr){
            _loadingDialog->hide();
            _loadingDialog->deleteLater();
            _loadingDialog = nullptr;
        }
        _send_pending = false;
    }
}

void SearchList::addTipItem()
{
    auto *invalid_item = new QWidget();
    QListWidgetItem *item_tmp = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item_tmp->setSizeHint(QSize(250,10));
    this->addItem(item_tmp);
    invalid_item->setObjectName("invalid_item");
    this->setItemWidget(item_tmp, invalid_item);
    item_tmp->setFlags(item_tmp->flags() & ~Qt::ItemIsSelectable);

    auto *add_user_item = new AddUserItem();
    QListWidgetItem *item = new QListWidgetItem;
    //qDebug()<<"chat_user_wid sizeHint is " << chat_user_wid->sizeHint();
    item->setSizeHint(add_user_item->sizeHint());
    this->addItem(item);
    this->setItemWidget(item, add_user_item);
}

void SearchList::slot_item_clicked(QListWidgetItem *item)
{
    QWidget* widget = this->itemWidget(item);
    if(!widget){
        ADDMSG(ElaMessageBarType::Top,"操作失败！",this,0);
        return ;
    }
    ListItemBase *customItem = qobject_cast<ListItemBase*>(widget);
    if(!customItem){
        ADDMSG(ElaMessageBarType::Top,"操作失败！",this,0);
        return ;
    }

    auto itemType = customItem->GetItemType();
    if(itemType == ListItemType::INVALID_ITEM){
        ADDMSG(ElaMessageBarType::Top,"操作失败！",this,0);
        return;
    }

    if(itemType == ListItemType::ADD_USER_TIP_ITEM){
        if(_send_pending){
            return ;
        }
        if(!_search_edit){
            return;
        }
        waitPending(true);

        auto search_edit = dynamic_cast<C_SearchEdit*>(_search_edit);
        auto uid_str = search_edit->text();
        QJsonObject obj;
        obj["uid"] = uid_str;
        QJsonDocument doc(obj);
        QByteArray byarr = doc.toJson(QJsonDocument::Compact);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_SEARCH_USER_REQ,byarr);

        return;
    }

    CloseFindDlg();
}

// 添加好友界面触发
void SearchList::slot_user_search(std::shared_ptr<SearchInfo> si)
{
    if (!_send_pending) {
        return;
    }

    waitPending(false);
    if(si == nullptr){
        ADDMSG(ElaMessageBarType::Top,"无法找到该用户，请检查输入是否正确！",this,0,3000);
        return;
    } else {
        // 添加自己为好友
        auto self_uid = UserMgr::GetInstance()->GetUid();
        if (self_uid == si->_uid){
            ADDMSG(ElaMessageBarType::Top,"不能添加自己为好友！",this,0,3000);
            return;
        }

        // 查询到已经是好友 todo...
        bool bExist = UserMgr::GetInstance()->CheckFriendById(si->_uid);
        if (bExist){
            // 已经是好友了，跳转好友界面 todo...
            emit sig_jump_chat_item(si);
            return;
        }

        // 未添加好友

    }

    if (_find_dlg) {
        _find_dlg->hide();
        _find_dlg->deleteLater();
        _find_dlg = nullptr;
    }

    QTimer::singleShot(50, this, [this, si]() {
        _find_dlg = std::make_shared<FindSuccessDlg>(this);
        std::dynamic_pointer_cast<FindSuccessDlg>(_find_dlg)->SetSearchInfo(si);
        _find_dlg->show();
    });
}
