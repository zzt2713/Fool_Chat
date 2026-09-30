#include "chatitembase.h"
#include "pixmaputil.h"
#include "themedtext.h"
#include <QTextEdit>
#include <QContextMenuEvent>

ChatItemBase::ChatItemBase(ChatRole role, QWidget *parent):QWidget(parent),_role(role)
{
    // 用户昵称
    _pNameLabel = new ElaText();
    _pNameLabel->setObjectName("chat_user_name");
    // 不设 TextStyle 则颜色不跟主题走，暗黑下会是黑字
    _pNameLabel->setTextStyle(ElaTextType::Body);
    BindTextToTheme(_pNameLabel);
    QFont font("Microsoft YaHei");
    font.setPointSize(9);
    _pNameLabel->setFont(font);
    _pNameLabel->setFixedHeight(20);

    // 用户头像
    _pIconLabel = new QLabel();
    _pIconLabel->setScaledContents(false);
    _pIconLabel->setFixedSize(42,42);
    _pIconLabel->installEventFilter(this);

    _pBubble = new QWidget();

    QGridLayout *pGLayout = new QGridLayout();
    pGLayout->setVerticalSpacing(3);
    pGLayout->setHorizontalSpacing(3);
    pGLayout->setContentsMargins(3,3,3,3);

    QSpacerItem *pSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    if(_role == ChatRole::Self)
    {
        // 自己发的消息不显示昵称，隐藏而非置空，避免留下 20px 空行
        _pNameLabel->hide();
        pGLayout->addWidget(_pNameLabel, 0,1, 1,1);
        pGLayout->addWidget(_pIconLabel, 0, 2, 2,1, Qt::AlignTop);
        pGLayout->addItem(pSpacer, 1, 0, 1, 1);
        pGLayout->addWidget(_pBubble, 1,1, 1,1);
        pGLayout->setColumnStretch(0, 2);
        pGLayout->setColumnStretch(1, 3);
    }else{
        _pNameLabel->setContentsMargins(8,0,0,0);
        _pNameLabel->setAlignment(Qt::AlignLeft);
        pGLayout->addWidget(_pIconLabel, 0, 0, 2,1, Qt::AlignTop);
        pGLayout->addWidget(_pNameLabel, 0,1, 1,1);
        pGLayout->addWidget(_pBubble, 1,1, 1,1);
        pGLayout->addItem(pSpacer, 2, 2, 1, 1);
        pGLayout->setColumnStretch(1, 3);
        pGLayout->setColumnStretch(2, 2);
    }
    this->setLayout(pGLayout);
}

void ChatItemBase::setUserName(const QString &name)
{
    _pNameLabel->setText(name);
}

void ChatItemBase::setUserIcon(const QPixmap &icon)
{
    // 高保真圆图，避免 setScaledContents 二次拉伸发糊
    _pIconLabel->setPixmap(PixmapUtil::round(icon, QSize(42, 42), devicePixelRatioF()));
}

void ChatItemBase::setUserId(int uid)
{
    _userId = uid;
}

void ChatItemBase::setFontScale(qreal scale)
{
    for (QTextEdit* edit : _pBubble->findChildren<QTextEdit*>()) {
        QFont f = edit->font();
        if (_baseFontPt <= 0) {
            _baseFontPt = f.pointSizeF() > 0 ? f.pointSizeF() : 9.0;
        }
        f.setPointSizeF(qMax(6.0, _baseFontPt * scale));
        edit->setFont(f);
    }
}

void ChatItemBase::setSendStatus(int st)
{
    if (_pStatus == nullptr) {
        _pStatus = new ElaText(this);
        _pStatus->setWordWrap(false);
        _pStatus->setTextPixelSize(10);
        _pStatus->installEventFilter(this);
        auto* grid = qobject_cast<QGridLayout*>(layout());
        if (grid != nullptr) {
            grid->addWidget(_pStatus, 2, 1, 1, 1,
                            _role == ChatRole::Self ? Qt::AlignRight : Qt::AlignLeft);
        }
    }
    _sendStatus = st;
    if (st == 0) {
        _pStatus->setText(QStringLiteral("发送中…"));
        _pStatus->setStyleSheet("color: rgba(128, 128, 128, 0.9);");
    }
    else if (st == 1) {
        _pStatus->setText(QStringLiteral("已送达"));
        _pStatus->setStyleSheet("color: rgba(128, 128, 128, 0.9);");
    }
    else {
        _pStatus->setText(QStringLiteral("发送失败，点击重试"));
        _pStatus->setStyleSheet("color: #ff4d4f;");
        _pStatus->setCursor(Qt::PointingHandCursor);
    }
}

QString ChatItemBase::textContent() const
{
    for (QTextEdit* edit : _pBubble->findChildren<QTextEdit*>()) {
        return edit->toPlainText();
    }
    return QString();
}

void ChatItemBase::contextMenuEvent(QContextMenuEvent* event)
{
    emit sigContextMenuRequested(event->globalPos(), this);
    event->accept();
}

bool ChatItemBase::eventFilter(QObject* watched, QEvent* event)
{
    // 点击"发送失败"状态文字 → 重试
    if (watched == _pStatus && event->type() == QEvent::MouseButtonPress && _sendStatus == 2) {
        emit sigRetrySend(this);
        return true;
    }
    // 气泡内部任意位置右键 → 统一弹条目菜单（拦截后抑制 QTextEdit 自带菜单）
    if (event->type() == QEvent::ContextMenu && _pBubble != nullptr &&
        qobject_cast<QWidget*>(watched) != nullptr &&
        (watched == _pBubble || _pBubble->isAncestorOf(qobject_cast<QWidget*>(watched)))) {
        auto* ce = static_cast<QContextMenuEvent*>(event);
        emit sigContextMenuRequested(ce->globalPos(), this);
        return true;
    }
    if (watched == _pIconLabel && event->type() == QEvent::MouseButtonPress) {
        if (_userId > 0) {
            emit sigIconClicked(_userId);
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ChatItemBase::setWidget(QWidget *w)
{
    QGridLayout *pGLayout = (qobject_cast<QGridLayout *>)(this->layout());
    pGLayout->replaceWidget(_pBubble, w);
    delete _pBubble;
    _pBubble = w;
    // 右键会被气泡内部控件（QTextEdit 等）消费，装过滤器统一截获
    if (_pBubble != nullptr) {
        _pBubble->installEventFilter(this);
        for (QWidget* child : _pBubble->findChildren<QWidget*>()) {
            child->installEventFilter(this);
        }
    }
}
