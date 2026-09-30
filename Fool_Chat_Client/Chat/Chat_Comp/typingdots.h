#ifndef TYPINGDOTS_H
#define TYPINGDOTS_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include "bubbleframe.h"

// 三个圆点依次弹跳的等待动画（样式对齐后台网页 .ai-thinking-dots）
class TypingDotsWidget : public QWidget
{
public:
    explicit TypingDotsWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QTimer* _timer;
    QElapsedTimer _clock;
};

// "AI 正在输入"占位气泡：内容区就是三个弹跳圆点
class TypingDotsBubble : public BubbleFrame
{
public:
    explicit TypingDotsBubble(ChatRole role, QWidget* parent = nullptr);
};

#endif // TYPINGDOTS_H
