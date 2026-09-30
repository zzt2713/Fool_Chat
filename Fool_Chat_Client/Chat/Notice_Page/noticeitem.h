#ifndef NOTICEITEM_H
#define NOTICEITEM_H

#include <QWidget>
#include "../Chat_Comp/listitembase.h"
#include "../Chat_Comp/userdata.h"

namespace Ui {
class NoticeItem;
}

class NoticeItem : public ListItemBase
{
    Q_OBJECT

public:
    explicit NoticeItem(QWidget *parent = nullptr);
    ~NoticeItem();
    QSize sizeHint() const override ;
    void SetInfo(std::shared_ptr<NoticeInfo> info);
    std::shared_ptr<NoticeInfo> GetInfo();
    // 本地置为已读（视觉置灰 + 收起对勾按钮）
    void MarkRead();

private:
    Ui::NoticeItem *ui;
    std::shared_ptr<NoticeInfo> _info;

private slots:
    void slot_read(QString);
signals:
    // 点击对勾：请求标记已读（source 0公告 1管理通知，id 表主键）
    void sig_read(int source, int id);
};

#endif // NOTICEITEM_H
