#include "typingdots.h"
#include <QPainter>
#include <cmath>

namespace {
// 网页 .ai-thinking-dots 参数：7px 圆点、5px 间距、1.2s 周期、逐点 0.15s 延迟、上抛 5px
constexpr int kDotSize = 7;
constexpr int kDotGap = 5;
constexpr qreal kCycleSec = 1.2;
constexpr qreal kDelaySec = 0.15;
constexpr qreal kPeakOffset = 5.0;
const QColor kDotColor(102, 112, 133); // 后台网页 --muted
} // namespace

TypingDotsWidget::TypingDotsWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(kDotSize * 3 + kDotGap * 2);
    // 静止位置 y=kPeakOffset，顶点 y=0，底部留 2px
    setFixedHeight(kDotSize + static_cast<int>(kPeakOffset) + 2);
    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, [this]() { update(); });
    _timer->start(33);
    _clock.start();
}

void TypingDotsWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const qreal secs = _clock.elapsed() / 1000.0;
    for (int i = 0; i < 3; ++i) {
        // 关键帧：0→0.3s 上抛到顶（不透明度 1），0.3→0.6s 回落（0.3），0.6→1.2s 停顿
        qreal t = std::fmod(secs - i * kDelaySec + kCycleSec, kCycleSec) / kCycleSec;
        qreal offY = 0.0;
        qreal alpha = 0.3;
        if (t < 0.3) {
            const qreal k = t / 0.3;
            offY = -kPeakOffset * k;
            alpha = 0.3 + 0.7 * k;
        }
        else if (t < 0.6) {
            const qreal k = (t - 0.3) / 0.3;
            offY = -kPeakOffset * (1.0 - k);
            alpha = 1.0 - 0.7 * k;
        }
        QColor c = kDotColor;
        c.setAlphaF(alpha);
        p.setBrush(c);
        const qreal x = i * (kDotSize + kDotGap);
        p.drawEllipse(QRectF(x, kPeakOffset + offY, kDotSize, kDotSize));
    }
}

TypingDotsBubble::TypingDotsBubble(ChatRole role, QWidget* parent)
    : BubbleFrame(role, parent)
{
    auto* dots = new TypingDotsWidget(this);
    setWidget(dots);
    // 紧贴三个点的尺寸，不被网格列拉宽
    const QMargins m = this->layout()->contentsMargins();
    setFixedHeight(dots->height() + m.top() + m.bottom());
    setMaximumWidth(dots->width() + m.left() + m.right());
}
