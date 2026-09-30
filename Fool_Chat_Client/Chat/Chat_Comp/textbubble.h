#ifndef TEXTBUBBLE_H
#define TEXTBUBBLE_H

#include <QTextEdit>
#include "bubbleframe.h"
#include <QHBoxLayout>

class TextBubble : public BubbleFrame
{
    Q_OBJECT
public:
    TextBubble(ChatRole role, const QString &text, QWidget *parent = nullptr);
    // 按 Markdown 渲染文本（AI 回复：列表/加粗/代码块），宽度高度同步重算
    void setMarkdownText(const QString& text);
protected:
    bool eventFilter(QObject *o, QEvent *e);
private:
    void adjustTextHeight();
    void setPlainText(const QString &text);
    // 按文档理想宽度收缩气泡最大宽度
    void updateMaxWidth();
private:
    QTextEdit *m_pTextEdit;
};

#endif // TEXTBUBBLE_H
