#include "applyfrienditem.h"
#include "ui_applyfrienditem.h"
#include "themedtext.h"
#include <QPainter>
#include <QPainterPath>

// 提高图片清晰度
static QPixmap getRoundPixmap(const QPixmap &src, QSize size, qreal dpr)
{
    if (src.isNull() || size.isEmpty()) return src;

    int diameter = qMin(size.width(), size.height());
    if (diameter <= 0) return src;

    QSize logicalSize(diameter, diameter);
    QSize physicalSize = logicalSize * dpr;

    QPixmap scaledSrc = src.scaled(physicalSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    scaledSrc.setDevicePixelRatio(dpr);

    QPixmap roundPixmap(physicalSize);
    roundPixmap.fill(Qt::transparent);
    roundPixmap.setDevicePixelRatio(dpr);

    QPainter painter(&roundPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addEllipse(0, 0, diameter, diameter);
    painter.setClipPath(path);

    qreal x = (diameter - scaledSrc.width() / dpr) / 2.0;
    qreal y = (diameter - scaledSrc.height() / dpr) / 2.0;
    painter.drawPixmap(QPointF(x, y), scaledSrc);

    return roundPixmap;
}

ApplyFriendItem::ApplyFriendItem(QWidget *parent)
    : ListItemBase(parent)
    , ui(new Ui::ApplyFriendItem),_added(false)
{
    ui->setupUi(this);
    SetItemType(ListItemType::APPLY_FRIEND_ITEM);
    BindTextToTheme(ui->user_name_lb);
    BindMutedTextToTheme(ui->user_chat_lb);
    ui->already_add_lb->setStyleSheet(QStringLiteral("color: rgba(128, 128, 128, 0.9);"));
    ui->addBtn->hide();
    connect(ui->addBtn, &ClikedBtn::clicked,  [this](){
        emit this->sig_auth_friend(_apply_info);
    });
}

ApplyFriendItem::~ApplyFriendItem()
{
    delete ui;
}

QSize ApplyFriendItem::sizeHint() const
{
    return QSize(250, 80); // 返回自定义的尺寸
}

void ApplyFriendItem::SetInfo(std::shared_ptr<ApplyInfo> apply_info)
{
    _apply_info = apply_info;
    // 加载图片
    QPixmap pixmap(_apply_info->_icon);
    qreal dpr = this->devicePixelRatioF();
    ui->icon_lb->setPixmap(getRoundPixmap(pixmap, ui->icon_lb->size(), dpr));
    // 账号为主，昵称不同则括号补充
    QString nameText = _apply_info->_name;
    if (!_apply_info->_nick.isEmpty() && _apply_info->_nick != _apply_info->_name) {
        nameText += QStringLiteral("（") + _apply_info->_nick + QStringLiteral("）");
    }
    ui->user_name_lb->setText(nameText);
    ui->user_chat_lb->setText(_apply_info->_desc);
}

void ApplyFriendItem::ShowAddBtn(bool bshow)
{
    if (bshow) {
        ui->addBtn->show();
        ui->already_add_lb->hide();
        _added = false;
    }
    else {
        ui->addBtn->hide();
        ui->already_add_lb->setText(QStringLiteral("已同意"));
        ui->already_add_lb->show();
        _added = true;
    }
}

int ApplyFriendItem::GetUid() {
    return _apply_info->_uid;
}
