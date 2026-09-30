#include "notifypopup.h"
#include "Chat_Comp/pixmaputil.h"

#include <QApplication>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>

namespace
{
constexpr int kCardWidth = 300;  // 卡片宽
constexpr int kCardHeight = 76;  // 卡片高
constexpr int kMargin = 18;      // 距屏幕右/下边距
constexpr int kGap = 10;         // 卡片垂直间距
constexpr int kMaxCards = 4;     // 同时显示上限（超出挤掉最旧）
constexpr int kDismissMs = 5000; // 自动消失时长
constexpr int kTickMs = 50;      // 倒计时步进
constexpr int kAvatarSize = 44;  // 头像边长
constexpr int kEnterOffset = 60; // 入场滑入位移（从右向左）
} // namespace

NotifyPopup::NotifyPopup(QObject* parent)
    : QObject(parent), _anchor(qobject_cast<QWidget*>(parent))
{
}

NotifyPopup::~NotifyPopup()
{
    // 卡片是无父顶层窗，随控制器销毁，避免主窗口关闭后残留
    for (NotifyCard* card : _cards) {
        card->close();
        delete card;
    }
    _cards.clear();
}

void NotifyPopup::notify(int kind, int uid, const QString& title,
                         const QString& body, const QString& iconPath)
{
    // 同人连发：复用已有卡片刷新内容，不叠加新卡
    for (NotifyCard* card : _cards) {
        if (card->canMerge(kind, uid)) {
            card->refresh(title, body);
            return;
        }
    }

    NotifyCard* card = new NotifyCard(kind, uid, title, body, loadAvatar(iconPath));
    connect(card, &NotifyCard::clicked, this, &NotifyPopup::sigNoticeClicked);
    connect(card, &NotifyCard::expired, this, &NotifyPopup::removeCard);

    _cards.insert(0, card);
    evictOverflow();
    relayout();

    // 必须先 show 再启动入场动画，否则动画不执行
    card->show();
    card->startEntrance();
}

QPixmap NotifyPopup::loadAvatar(const QString& iconPath) const
{
    QPixmap pix(iconPath);
    if (pix.isNull()) {
        pix.load(":/icons/image.png");
    }
    if (!pix.isNull()) {
        pix = PixmapUtil::round(pix, QSize(kAvatarSize, kAvatarSize),
                                 _anchor ? _anchor->devicePixelRatioF() : 1.0);
    }
    return pix;
}

void NotifyPopup::evictOverflow()
{
    while (_cards.size() > kMaxCards) {
        NotifyCard* oldest = _cards.takeLast();
        oldest->hide();
        oldest->deleteLater();
    }
}

void NotifyPopup::relayout()
{
    QRect available;
    QScreen* screen = _anchor ? _anchor->screen() : QGuiApplication::primaryScreen();
    if (screen != nullptr) {
        available = screen->availableGeometry();
    }

    for (int i = 0; i < _cards.size(); ++i) {
        // i = 0 最新在最底，向上依次堆叠
        const QPoint target(available.right() - kMargin - kCardWidth,
                            available.bottom() - kMargin - kCardHeight - i * (kCardHeight + kGap));
        _cards.at(i)->moveTo(target);
    }
}

void NotifyPopup::removeCard(NotifyCard* card)
{
    if (!_cards.removeOne(card)) {
        return;
    }
    card->deleteLater();
    relayout();
}

NotifyCard::NotifyCard(int kind, int uid, const QString& title, const QString& body,
                       const QPixmap& avatar, QWidget* parent)
    : QWidget(parent), _kind(kind), _uid(uid), _title(title), _avatar(avatar)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(kCardWidth, kCardHeight);
    setCursor(Qt::PointingHandCursor);

    // 单行预览，进入时按可用宽度截断
    QFont bodyFont = font();
    bodyFont.setPixelSize(13);
    _body = QFontMetrics(bodyFont).elidedText(body, Qt::ElideRight,
                                               kCardWidth - kAvatarSize - 60);

    _dismissTimer = new QTimer(this);
    _dismissTimer->setInterval(kTickMs);
    connect(_dismissTimer, &QTimer::timeout, this, [this]() {
        if (_hovered) {
            return;
        }
        _remaining -= kTickMs;
        if (_remaining <= 0) {
            startDismiss();
        }
    });
    _dismissTimer->start();
}

bool NotifyCard::canMerge(int kind, int uid) const
{
    return !_dismissing && _kind == kind && _uid == uid;
}

void NotifyCard::refresh(const QString& title, const QString& body)
{
    _title = title;
    _count++;

    QFont bodyFont = font();
    bodyFont.setPixelSize(13);
    _body = QFontMetrics(bodyFont).elidedText(body, Qt::ElideRight,
                                               kCardWidth - kAvatarSize - 60);

    // 重置倒计时，给合并进来的消息完整的阅读时间
    _remaining = kDismissMs;
    _dismissTimer->start();
    update();
}

void NotifyCard::moveTo(const QPoint& targetPos)
{
    if (pos() == targetPos) {
        return;
    }
    if (!isVisible()) {
        move(targetPos);
        return;
    }
    QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(200);
    anim->setStartValue(pos());
    anim->setEndValue(targetPos);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotifyCard::startEntrance()
{
    setWindowOpacity(0.0);
    move(pos() + QPoint(kEnterOffset, 0));

    QPropertyAnimation* moveAnim = new QPropertyAnimation(this, "pos");
    moveAnim->setDuration(300);
    moveAnim->setStartValue(this->pos());
    moveAnim->setEndValue(this->pos() - QPoint(kEnterOffset, 0));
    moveAnim->setEasingCurve(QEasingCurve::OutCubic);

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(this, "windowOpacity");
    opacityAnim->setDuration(300);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    moveAnim->start(QAbstractAnimation::DeleteWhenStopped);
    opacityAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotifyCard::startDismiss()
{
    if (_dismissing) {
        return;
    }
    _dismissing = true;
    _dismissTimer->stop();

    QPropertyAnimation* opacityAnim = new QPropertyAnimation(this, "windowOpacity");
    opacityAnim->setDuration(200);
    opacityAnim->setStartValue(1.0);
    opacityAnim->setEndValue(0.0);
    opacityAnim->setEasingCurve(QEasingCurve::InCubic);
    connect(opacityAnim, &QPropertyAnimation::finished, this, [this]() {
        emit expired(this);
    });
    opacityAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotifyCard::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect mainRect = rect().adjusted(1, 1, -1, -1);

    // 双层阴影
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawRoundedRect(mainRect.adjusted(0, 2, 0, 2), 10, 10);
    painter.setBrush(QColor(0, 0, 0, 20));
    painter.drawRoundedRect(mainRect.adjusted(0, 1, 0, 1), 10, 10);

    // 背景
    painter.setBrush(QColor(255, 255, 255, 245));
    painter.setPen(QPen(QColor(0, 0, 0, 40), 1));
    painter.drawRoundedRect(mainRect, 10, 10);

    // 头像
    const int avatarX = 16;
    const int avatarY = (kCardHeight - kAvatarSize) / 2;
    if (!_avatar.isNull()) {
        painter.drawPixmap(avatarX, avatarY, _avatar);
    }
    else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 120, 215));
        painter.drawEllipse(avatarX, avatarY, kAvatarSize, kAvatarSize);
    }

    // 文字：标题一行 + 正文一行
    const int textX = avatarX + kAvatarSize + 12;
    const int textWidth = kCardWidth - textX - 16;

    QFont titleFont = font();
    titleFont.setPixelSize(14);
    titleFont.setBold(true);

    QFont bodyFont = font();
    bodyFont.setPixelSize(13);

    // 合并多条时标题带计数（微信样式）
    const QString titleText = _count > 1
                                  ? QStringLiteral("%1（%2条新消息）").arg(_title).arg(_count)
                                  : _title;

    painter.setPen(QColor(45, 45, 45));
    painter.setFont(titleFont);
    painter.drawText(QRect(textX, 14, textWidth, 22), Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(titleFont).elidedText(titleText, Qt::ElideRight, textWidth));

    painter.setPen(QColor(128, 128, 128));
    painter.setFont(bodyFont);
    painter.drawText(QRect(textX, 40, textWidth, 22), Qt::AlignLeft | Qt::AlignVCenter, _body);
}

void NotifyCard::enterEvent(QEnterEvent* event)
{
    _hovered = true;
    QWidget::enterEvent(event);
}

void NotifyCard::leaveEvent(QEvent* event)
{
    _hovered = false;
    QWidget::leaveEvent(event);
}

void NotifyCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !_dismissing) {
        emit clicked(_kind, _uid);
        startDismiss();
    }
    QWidget::mouseReleaseEvent(event);
}
