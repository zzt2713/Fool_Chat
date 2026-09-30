#include "bubbleframe.h"
#include <QPainter>
#include <QPainterPath>
#include "ElaTheme.h"

BubbleFrame::BubbleFrame(ChatRole role, QWidget *parent)
    : QFrame(parent)
    , m_role(role)
    , m_margin(8) // 上下边距
{
    m_pHLayout = new QHBoxLayout();

    const int TEXT_PADDING_H = 10;

    if(m_role == ChatRole::Self)
    {
        // 自己的消息
        m_pHLayout->setContentsMargins(
            TEXT_PADDING_H,
            m_margin,
            TEXT_PADDING_H - 2,
            m_margin
            );
    }
    else
    {
        // 对方的消息
        m_pHLayout->setContentsMargins(
            TEXT_PADDING_H - 2,
            m_margin,
            TEXT_PADDING_H,
            m_margin
            );
    }

    this->setLayout(m_pHLayout);
}

void BubbleFrame::setMargin(int margin)
{
    m_margin = margin;
}

void BubbleFrame::setWidget(QWidget *w)
{
    if(m_pHLayout->count() > 0)
        return ;
    else{
        m_pHLayout->addWidget(w);
    }
}

void BubbleFrame::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    QPainterPath path;
    QRectF bk_rect = QRectF(0, 0, this->width(), this->height());
    qreal radius = 13.0;

    // 气泡底色跟随主题（写死浅色的话暗黑下白字白底不可读）
    const bool dark = eTheme->getThemeMode() == ElaThemeType::Dark;
    if(m_role == ChatRole::Other)
    {
        painter.setBrush(dark ? QColor("#3C3C3C") : QColor("#FFFFFF"));
    }
    else
    {
        painter.setBrush(dark ? QColor("#2E5F8F") : QColor("#D9EAFA"));
    }

    path.addRoundedRect(bk_rect, radius, radius);
    painter.drawPath(path);

    QFrame::paintEvent(e);
}
