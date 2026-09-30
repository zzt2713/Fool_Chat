#ifndef CHATUSERLIST_H
#define CHATUSERLIST_H

#include <QListWidget>
#include <QWheelEvent>
#include <QEvent>
#include <QScrollBar>
#include <QDebug>

class QLabel;

class ChatUserList: public QListWidget
{
    Q_OBJECT
public:
    explicit ChatUserList(QWidget *parent = nullptr);

private:
    bool _load_pending;
    QLabel* _emptyLabel{nullptr}; // 空列表占位提示
    void updateEmptyLabel();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void sig_loading_chat_user();
};

#endif // CHATUSERLIST_H
