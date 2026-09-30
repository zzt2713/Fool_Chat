#include "msg_notice.h"
#include "ui_msg_notice.h"
#include "noticeitem.h"
#include <QScrollBar>
#include <QEvent>
#include <QWheelEvent>
#include <QJsonObject>
#include <QLabel>
#include <memory>

Msg_Notice::Msg_Notice(QWidget *parent)
    : QListWidget(parent)
    , ui(new Ui::Msg_Notice)
{
    ui->setupUi(this);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 安装事件过滤器
    this->viewport()->installEventFilter(this);

    _emptyLabel = new QLabel("暂无通知", this->viewport());
    _emptyLabel->setAlignment(Qt::AlignCenter);
    _emptyLabel->setStyleSheet("color: rgba(128, 128, 128, 0.9); font-size: 14px;");
    _emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(model(), &QAbstractItemModel::rowsInserted, this, [this]() { updateEmptyLabel(); });
    connect(model(), &QAbstractItemModel::rowsRemoved, this, [this]() { updateEmptyLabel(); });
    updateEmptyLabel();
}

void Msg_Notice::updateEmptyLabel()
{
    _emptyLabel->setGeometry(viewport()->rect());
    _emptyLabel->setVisible(count() == 0);
}

void Msg_Notice::resizeEvent(QResizeEvent* event)
{
    QListWidget::resizeEvent(event);
    updateEmptyLabel();
}

Msg_Notice::~Msg_Notice()
{
    delete ui;
}

bool Msg_Notice::eventFilter(QObject *watched, QEvent *event)
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

        if (maxScrollValue - currentValue <= 0) {
            emit sig_loading_Notice_Msg();
        }

        return true; // 停止事件传递
    }

    return QListWidget::eventFilter(watched, event);
}

void Msg_Notice::SetNotices(const QJsonArray& notices)
{
    this->clear();

    for (const QJsonValue& value : notices) {
        QJsonObject obj = value.toObject();
        auto notice = std::make_shared<NoticeInfo>(
            obj["id"].toInt(),
            obj["source"].toInt(),
            obj["title"].toString(),
            obj["author"].toString(),
            obj["content"].toString(),
            obj["create_time"].toString(),
            obj["level"].toString(),
            obj["delivered"].toInt()
            );

        QListWidgetItem *item = new QListWidgetItem;
        NoticeItem *noticeItem = new NoticeItem;
        connect(noticeItem, &NoticeItem::sig_read, this, &Msg_Notice::sig_read_notice);
        noticeItem->SetInfo(notice);
        item->setSizeHint(noticeItem->sizeHint());
        this->addItem(item);
        this->setItemWidget(item, noticeItem);
    }
}
