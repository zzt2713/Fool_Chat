#include "noticeitem.h"
#include "ui_noticeitem.h"
#include "pixmaputil.h"
#include <QFontMetrics>

NoticeItem::NoticeItem(QWidget *parent)
    : ListItemBase(parent)
    , ui(new Ui::NoticeItem)
{
    ui->setupUi(this);
    // 公告/通知统一用系统图标，不再使用测试头像
    QPixmap iconPixmap(":/icons/image.png");
    qreal dpr = this->devicePixelRatioF();
    ui->icon_lb->setPixmap(PixmapUtil::round(iconPixmap, ui->icon_lb->size(), dpr));
    ui->icon_lb->setScaledContents(false);

    ui->read_lb->setElaIcon(ElaIconType::Check);
    ui->read_lb->setToolTip("标记已读");
    this->setFocusPolicy(Qt::NoFocus);
    this->SetItemType(ListItemType::NOTICE_MSG_ITEM);
    connect(ui->read_lb,&ClickedOnceLabel::clicked,this,&NoticeItem::slot_read);
}

NoticeItem::~NoticeItem()
{
    delete ui;
}

QSize NoticeItem::sizeHint() const
{
    return QSize(200,80);
}

void NoticeItem::SetInfo(std::shared_ptr<NoticeInfo> info)
{
    _info = info;

    // 等级色点：info 蓝 / warning 橙 / success 绿，标题一眼分级
    QString plainTitle = QFontMetrics(ui->user_name_lb->font()).elidedText(_info->_title, Qt::ElideRight, 180);
    QString title = plainTitle;
    if (!_info->_level.isEmpty()) {
        QString dotColor = "#409EFF";
        if (_info->_level == "warning") {
            dotColor = "#E6A23C";
        }
        else if (_info->_level == "success") {
            dotColor = "#67C23A";
        }
        title = QString("<span style='color:%1'>&#9679;</span> %2").arg(dotColor, plainTitle);
    }
    ui->user_name_lb->setText(title);
    setToolTip(_info->_title);

    QString sub = _info->_create_time;
    if (!_info->_author.isEmpty()) {
        sub += "  " + _info->_author;
    }
    sub += "  " + _info->_content;
    // 长文本省略号截断，完整内容放悬停提示
    ui->msg_lb->setText(QFontMetrics(ui->msg_lb->font()).elidedText(sub, Qt::ElideRight, 400));
    ui->msg_lb->setToolTip(_info->_content);

    if (_info->_delivered == 1) {
        MarkRead();
    }
}

std::shared_ptr<NoticeInfo> NoticeItem::GetInfo()
{
    return _info;
}

void NoticeItem::MarkRead()
{
    if (!_info) {
        return;
    }
    _info->_delivered = 1;
    ui->read_lb->hide();
    ui->user_name_lb->setStyleSheet("color: gray;");
    ui->msg_lb->setStyleSheet("color: gray;");
}

void NoticeItem::slot_read(QString)
{
    if (!_info || _info->_delivered == 1) {
        return;
    }
    emit sig_read(_info->_source, _info->_id);
    MarkRead();
}
