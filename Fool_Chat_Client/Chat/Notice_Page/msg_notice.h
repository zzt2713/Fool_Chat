#ifndef MSG_NOTICE_H
#define MSG_NOTICE_H

#include <QListWidget>
#include <QJsonArray>

class QLabel;

namespace Ui {
class Msg_Notice;
}

class Msg_Notice : public QListWidget
{
    Q_OBJECT

public:
    explicit Msg_Notice(QWidget *parent = nullptr);
    ~Msg_Notice();
    // 用服务端通知列表重建条目
    void SetNotices(const QJsonArray& notices);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::Msg_Notice *ui;
    QLabel* _emptyLabel{nullptr}; // 空列表占位提示
    void updateEmptyLabel();

signals:
    void sig_loading_Notice_Msg();
    // 条目对勾点击：转发给通知页请求标记已读
    void sig_read_notice(int source, int id);
};

#endif // MSG_NOTICE_H
