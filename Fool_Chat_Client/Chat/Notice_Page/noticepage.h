#ifndef NOTICEPAGE_H
#define NOTICEPAGE_H
/******************************************************************************
*
* @file       noticepage.h
* @brief      通知页 Function
*
* @author     Fool
* @date       2026/03/03
* @history
*****************************************************************************/
#include "../Chat_Comp/page_base.h"
#include "msg_notice.h"
#include <QJsonArray>

class ElaSelectorBar;

class NoticePage:public Page_Base
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit NoticePage(QWidget* parent = nullptr);
    ~NoticePage();
    void loadData();

signals:
    void sig_num_msg(QWidget*,int);

private slots:
    void slot_notice_list(QJsonArray notices);
    void slot_read_notice(int source, int id);
    void slot_filter_changed(int index);
    // 双击条目：弹窗查看公告详情
    void slot_show_detail(QListWidgetItem* item);

private:
    int countUnread() const;
    // 一键已读：全部未读条目标已读并通知服务端
    void readAllNotices();

    ElaSelectorBar* _selector{nullptr}; // 全部/未读/已读筛选
    Msg_Notice* _noticeList{nullptr};   // 通知条目列表
    int _filter{0};                     // 当前筛选（0全部 1未读 2已读）
};
#endif // NOTICEPAGE_H
