#ifndef CHATVIEW_H
#define CHATVIEW_H
#include <QWidget>
#include <QVBoxLayout>
#include "ElaScrollArea.h"

class ChatView:public QWidget
{
    Q_OBJECT
public:
    explicit ChatView(QWidget* parent = Q_NULLPTR);
    ~ChatView();
    void appendItem(QWidget* item);
    void prependItem(QWidget* item);
    void insertItem(QWidget* before,QWidget* item);
    void removeItem(QWidget* item);
    void removeAllItem();
    // 枚举当前气泡条目（Ctrl+滚轮缩放字体时遍历用）
    QList<QWidget*> items() const;
signals:
    // Ctrl+滚轮：true 放大 false 缩小
    void sigFontZoom(bool zoomIn);
protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void paintEvent(QPaintEvent *event) override;
private slots:
    void onVScrollBarMoved(int min, int max);
private:
    void initStyleSheet();
    QVBoxLayout *m_pVl;
    ElaScrollArea *m_pScrollArea;
    bool isAppended;
};
#endif // CHATVIEW_H
