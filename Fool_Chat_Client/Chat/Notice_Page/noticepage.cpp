#include "noticepage.h"
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QJsonDocument>
#include <QJsonObject>
#include "../Chat_Comp/applyfriendpage.h"
#include "ElaSelectorBar.h"
#include "ElaPushButton.h"
#include "ElaDialog.h"
#include "ElaText.h"
#include "noticeitem.h"
#include "tcpmgr.h"
#include "usermgr.h"

NoticePage::NoticePage(QWidget *parent):Page_Base(parent)
{
    setWindowTitle("通知");
    QWidget* content = new QWidget();
    content->setWindowTitle("通知");
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    _selector = new ElaSelectorBar(this);
    _selector->addItem("全部消息");
    _selector->addItem("未读消息");
    _selector->addItem("已读消息");

    ElaPushButton* readAllBtn = new ElaPushButton("一键已读", this);
    readAllBtn->setFixedHeight(32);
    // 撑出最小宽度，避免文字紧贴按钮边（与其它按钮 112 宽对齐）
    readAllBtn->setMinimumWidth(112);
    connect(readAllBtn, &ElaPushButton::clicked, this, &NoticePage::readAllNotices);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(_selector);
    headerLayout->addStretch();
    headerLayout->addWidget(readAllBtn);
    layout->addLayout(headerLayout);

    _noticeList = new Msg_Notice();

    _noticeList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(_noticeList);

    addCentralWidget(content);

    connect(_selector, &ElaSelectorBar::currentIndexChanged, this, &NoticePage::slot_filter_changed);
    connect(_noticeList, &Msg_Notice::sig_read_notice, this, &NoticePage::slot_read_notice);
    connect(_noticeList, &QListWidget::itemDoubleClicked, this, &NoticePage::slot_show_detail);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_notice_list, this, &NoticePage::slot_notice_list);
}

NoticePage::~NoticePage()
{

}

void NoticePage::loadData()
{
    QJsonObject obj;
    obj["uid"] = UserMgr::GetInstance()->GetUid();
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_NOTICES_REQ,
                                             QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void NoticePage::slot_notice_list(QJsonArray notices)
{
    UserMgr::GetInstance()->SetNoticeCache(notices);
    _noticeList->SetNotices(notices);
    slot_filter_changed(_filter);
    emit sig_num_msg(this, countUnread());
}

void NoticePage::slot_read_notice(int source, int id)
{
    QJsonObject obj;
    obj["source"] = source;
    obj["id"] = id;
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_READ_NOTICE_REQ,
                                             QJsonDocument(obj).toJson(QJsonDocument::Compact));
    slot_filter_changed(_filter);
    emit sig_num_msg(this, countUnread());
}

void NoticePage::slot_filter_changed(int index)
{
    _filter = index;
    for (int i = 0; i < _noticeList->count(); ++i) {
        QListWidgetItem* item = _noticeList->item(i);
        auto* noticeItem = qobject_cast<NoticeItem*>(_noticeList->itemWidget(item));
        if (!noticeItem || !noticeItem->GetInfo()) {
            continue;
        }
        bool delivered = noticeItem->GetInfo()->_delivered == 1;
        bool show = (index == 0) || (index == 1 && !delivered) || (index == 2 && delivered);
        item->setHidden(!show);
    }
}

void NoticePage::slot_show_detail(QListWidgetItem* item)
{
    auto* noticeItem = qobject_cast<NoticeItem*>(_noticeList->itemWidget(item));
    if (!noticeItem || !noticeItem->GetInfo()) {
        return;
    }
    auto info = noticeItem->GetInfo();

    // 详情弹窗
    ElaDialog* dlg = new ElaDialog(this->window());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("公告详情");
    dlg->setFixedSize(440, 380);
    dlg->setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    QVBoxLayout* lay = new QVBoxLayout(dlg);
    lay->setContentsMargins(30, 24, 30, 24);
    lay->setSpacing(12);

    ElaText* title = new ElaText(info->_title, dlg);
    title->setWordWrap(true);
    title->setTextPixelSize(18);
    lay->addWidget(title);

    QString meta = info->_create_time;
    if (!info->_author.isEmpty()) {
        meta += "    " + info->_author;
    }
    if (!info->_level.isEmpty()) {
        meta += "    " + info->_level;
    }
    ElaText* metaLb = new ElaText(meta, dlg);
    metaLb->setObjectName("notice_meta_lb");
    metaLb->setWordWrap(false);
    metaLb->setTextPixelSize(12);
    lay->addWidget(metaLb);

    ElaText* content = new ElaText(info->_content, dlg);
    content->setWordWrap(true);
    content->setTextPixelSize(14);
    content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(content, 1);

    dlg->moveToCenter();
    dlg->show();
}

void NoticePage::readAllNotices()
{
    for (int i = 0; i < _noticeList->count(); ++i) {
        auto* noticeItem = qobject_cast<NoticeItem*>(_noticeList->itemWidget(_noticeList->item(i)));
        if (!noticeItem || !noticeItem->GetInfo() || noticeItem->GetInfo()->_delivered == 1) {
            continue;
        }
        QJsonObject obj;
        obj["source"] = noticeItem->GetInfo()->_source;
        obj["id"] = noticeItem->GetInfo()->_id;
        noticeItem->MarkRead();
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_READ_NOTICE_REQ,
                                                 QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
    slot_filter_changed(_filter);
    emit sig_num_msg(this, countUnread());
}

int NoticePage::countUnread() const
{
    int unread = 0;
    for (int i = 0; i < _noticeList->count(); ++i) {
        auto* noticeItem = qobject_cast<NoticeItem*>(_noticeList->itemWidget(_noticeList->item(i)));
        if (noticeItem && noticeItem->GetInfo() && noticeItem->GetInfo()->_delivered == 0) {
            ++unread;
        }
    }
    return unread;
}
