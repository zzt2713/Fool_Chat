#include "floatingtip.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QDebug>
#include <QMouseEvent>

// 构造函数
FloatingTip::FloatingTip()
    : QWidget(nullptr) // 单例模式作为顶层窗口，构造时不指定parent
{
    // 1. 窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);

    // 关键属性：显示时不激活窗口（不抢焦点），防止卡顿
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 2. 字体设置
    QFont font("Microsoft YaHei", 10);
    this->setFont(font);

    // 3. 定时器初始化
    _animTimer = new QTimer(this);
    _totalTime = 2000;
    _curTime = _totalTime;
    setMouseTracking(true);
    connect(_animTimer, &QTimer::timeout, this, [=](){
        if(!_isHovered){
            _curTime -= 20;
            if (_curTime <= 0) {
                _animTimer->stop();
                this->hide(); // 单例只隐藏，不关闭
            } else {
                update();
            }
        }
    });

    setFixedSize(250, 50);
}
void FloatingTip::mouseMoveEvent(QMouseEvent *event)
{
    bool lastHoverState = _isHoveredCloseBtn;

    // 检查鼠标是否在关闭按钮区域内
    _isHoveredCloseBtn = _closeBtnRect.contains(event->pos());

    // 如果状态发生变化，重绘
    if (lastHoverState != _isHoveredCloseBtn) {
        update();
    }

    QWidget::mouseMoveEvent(event);
}

// 静态调用入口
void FloatingTip::showTip(QWidget *parent, const QString &text, bool isSuccess)
{
    FloatingTip* instance = FloatingTip::GetInstance().get();
    instance->resetContent(parent, text, isSuccess);

    // 必须先 show 再启动动画，否则动画可能不执行
    instance->show();
    instance->startEntranceAnimation();
    instance->_animTimer->start(20);
}

// 重置内容（核心修复点：移除 setParent）
void FloatingTip::resetContent(QWidget *parent, const QString &text, bool isSuccess)
{
    _animTimer->stop();
    _isHovered = false;
    _curTime = _totalTime;
    _text = text;
    _isSuccess = isSuccess;

    if (parent) {
        // 获取 parent 窗口在屏幕上的绝对位置
        QPoint globalPos = parent->mapToGlobal(QPoint(0, 0));
        int x = globalPos.x() + (parent->width() - this->width()) / 2;
        int y = globalPos.y() + 30;
        this->move(x, y);
    }
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPixelSize(16);
    QFontMetrics titleFm(titleFont);
    QString titleText = _isSuccess ? "成功   " :"错误 ";
    int titleWidth = titleFm.horizontalAdvance(titleText);

    // 2. 计算内容宽度
    QFont contentFont = font();
    contentFont.setPixelSize(14);
    QFontMetrics contentFm(contentFont);
    int contentWidth = contentFm.horizontalAdvance(text);

    // 3. 计算按钮区域宽度 (closeSize=26, margin=12)
    int closeBtnAreaWidth = 26 + 12;

    // 4. 计算总宽度
    // 布局结构: [左边距15] [图标20] [间距15] [标题] [间距5] [内容] [间距10] [按钮区域] [右边距10]
    int w = 15 + 20 + 15 + titleWidth + 5 + contentWidth + 10 + closeBtnAreaWidth + 10;

    // 设置最小宽度，防止太短
    if(w < 250) w = 250;

    int h = 45;
    this->setFixedSize(w, h + 5);

    update();
}

void FloatingTip::startEntranceAnimation()
{
    this->setWindowOpacity(1.0); // 重置透明度

    QPropertyAnimation *anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(400);
    anim->setStartValue(this->pos() - QPoint(0, 20));
    anim->setEndValue(this->pos());
    anim->setEasingCurve(QEasingCurve::OutCubic);

    QPropertyAnimation *opacityAnim = new QPropertyAnimation(this, "windowOpacity");
    opacityAnim->setDuration(400);
    opacityAnim->setStartValue(0.0);
    opacityAnim->setEndValue(1.0);

    anim->start(QAbstractAnimation::DeleteWhenStopped);
    opacityAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void FloatingTip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor themeColor = _isSuccess ? QColor(103, 194, 58) : QColor(245, 108, 108);

    // 背景色：成功时淡绿色，失败时淡红色
    QColor bgColor;
    if (_isSuccess) {
        bgColor = QColor(235, 255, 235);  // 淡绿色背景
        bgColor.setAlpha(240);
    } else {
        bgColor = QColor(255, 235, 235);  // 淡红色背景
        bgColor.setAlpha(240);
    }

    QColor textColor = QColor(60, 60, 60);
    QRect mainRect = rect();

    // 添加主阴影
    painter.save();
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(mainRect.adjusted(2, 2, 2, 2), 6, 6);
    painter.fillPath(shadowPath, QColor(0, 0, 0, 40));
    painter.restore();

    // 添加次要阴影
    painter.save();
    QPainterPath softShadowPath;
    softShadowPath.addRoundedRect(mainRect.adjusted(1, 1, 1, 1), 6, 6);
    painter.fillPath(softShadowPath, QColor(0, 0, 0, 20));
    painter.restore();

    // 背景
    QPainterPath bgPath;
    bgPath.addRoundedRect(mainRect, 6, 6);
    painter.fillPath(bgPath, bgColor);

    // 描边
    painter.save();
    painter.setPen(QPen(QColor(0, 0, 0, 50), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(bgPath);
    painter.restore();

    // --- 绘制图标 ---
    int iconSize = 20;
    int iconX = mainRect.left() + 15;
    int iconY = mainRect.center().y() - iconSize/2;

    painter.setBrush(themeColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(iconX, iconY, iconSize, iconSize);

    painter.setPen(QPen(Qt::white, 2.5));
    painter.setRenderHint(QPainter::Antialiasing, true);

    if(_isSuccess) {
        int margin = 5;
        QPointF points[3] = {
            QPointF(iconX + margin, iconY + iconSize/2),
            QPointF(iconX + iconSize/2 - 2, iconY + iconSize - margin),
            QPointF(iconX + iconSize - margin, iconY + margin)
        };
        QPainterPath checkPath;
        checkPath.moveTo(points[0]);
        checkPath.lineTo(points[1]);
        checkPath.lineTo(points[2]);
        painter.drawPath(checkPath);
    } else {
        painter.setPen(QPen(Qt::white, 2));
        QFont iconFont("Segoe UI", 16, QFont::Bold);
        painter.setFont(iconFont);
        painter.drawText(QRect(iconX, iconY, iconSize, iconSize), Qt::AlignCenter, "!");
    }

    // --- 绘制文字 ---
    painter.setPen(textColor);

    QFont baseFont = font();
    QFont titleFont = baseFont;
    titleFont.setBold(true);
    titleFont.setPixelSize(16);

    QFont contentFont = baseFont;
    contentFont.setPixelSize(14);

    QColor titleColor = _isSuccess ? QColor(46, 125, 50) : QColor(198, 40, 40);
    QString titleText = _isSuccess ? "成功   " : "错误   ";

    QFontMetrics titleFm(titleFont);
    int titleWidth = titleFm.horizontalAdvance(titleText);

    // 文字起始位置
    int textStartX = iconX + iconSize + 15;
    int textY = mainRect.top();
    int textH = mainRect.height();

    // 1. 绘制标题
    painter.save();
    painter.setPen(titleColor);
    painter.setFont(titleFont);
    painter.drawText(textStartX, textY, titleWidth, textH, Qt::AlignLeft | Qt::AlignVCenter, titleText);
    painter.restore();

    // 2. 绘制内容
    painter.setFont(contentFont);
    int contentStartX = textStartX + titleWidth + 5;

    // 注意：这里不限制绘制宽度，或者给一个足够大的宽度，因为我们在 resetContent 已经算好了窗口大小
    painter.drawText(QRect(contentStartX, textY, 2000, textH), Qt::AlignLeft | Qt::AlignVCenter, _text);

    // --- 绘制关闭按钮 (关键修改：计算位置) ---
    int closeSize = 26;
    int closeMargin = 10; // 按钮与右侧边距

    // 核心：计算文字实际结束的 X 坐标
    QFontMetrics contentFm(contentFont);
    int textEndX = contentStartX + contentFm.horizontalAdvance(_text);

    // X 按钮位置 = 文字结束位置 + 间距
    int closeX = textEndX + 10;

    // 安全检查：防止按钮跑出窗口右边 (虽然理论上 resetContent 已经保证了宽度，但防止意外)
    if (closeX + closeSize + closeMargin > mainRect.width()) {
        closeX = mainRect.width() - closeSize - closeMargin;
    }

    int closeY = mainRect.center().y() - closeSize / 2;
    _closeBtnRect = QRect(closeX, closeY, closeSize, closeSize);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    // 悬停背景
    if (_isHoveredCloseBtn) {
        QColor hoverBgColor = _isSuccess ? QColor(0, 100, 0, 30) : QColor(100, 0, 0, 30);
        painter.setBrush(hoverBgColor);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(_closeBtnRect, 4, 4);
    }

    // X 图标
    QPen xPen(QColor(80, 80, 80), 1.5, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(xPen);

    int padding = 8;
    QRectF iconRect = _closeBtnRect.adjusted(padding, padding, -padding, -padding);
    painter.drawLine(iconRect.topLeft(), iconRect.bottomRight());
    painter.drawLine(iconRect.topRight(), iconRect.bottomLeft());

    painter.restore();

    // --- 进度条 ---
    if (_curTime > 0) {
        double ratio = (double)_curTime / _totalTime;
        int barWidth = mainRect.width() * ratio;
        int barHeight = 3;
        int barY = mainRect.bottom() - barHeight;
        QRect barRect(mainRect.x(), barY, barWidth, barHeight);

        painter.save();
        painter.setClipPath(bgPath);
        painter.fillRect(barRect, themeColor);
        painter.restore();
    }
}

void FloatingTip::enterEvent(QEnterEvent *event)
{
    _isHovered = true;
    QWidget::enterEvent(event);
}

void FloatingTip::leaveEvent(QEvent *event)
{
    _isHovered = false;
    _isHoveredCloseBtn = false; // 明确重置
    update(); // 触发重绘以隐藏关闭按钮背景
    QWidget::leaveEvent(event);
}

void FloatingTip::mousePressEvent(QMouseEvent *event)
{
    if (_closeBtnRect.contains(event->pos())) {
        _animTimer->stop();
        hide();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingTip::closeEvent(QCloseEvent *event)
{
    event->ignore();
    _animTimer->stop();
    hide();
}
