#ifndef SEARCHLIST_H
#define SEARCHLIST_H
/******************************************************************************
*
* @file       searchlist.h
* @brief      搜索列表 Function
*
* @author     Fool
* @date       2026/03/09
* @history
*****************************************************************************/

#include "loadingdlg.h"
#include <QListWidget>
#include <QWheelEvent>
#include <QEvent>
#include <QScrollBar>
#include "ElaScrollBar.h"
#include <memory>
#include "userdata.h"
class FindSuccessDlg;
class SearchList: public QListWidget
{
    Q_OBJECT
public:
    SearchList(QWidget* parent = nullptr);
    void CloseFindDlg();
    void SetSearchEdit(QWidget* edit);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void waitPending(bool pending = true);
    void addTipItem();

    std::shared_ptr<QDialog> _find_dlg;
    // FindSuccessDlg* _find_dlg{nullptr};
    QWidget* _search_edit;
    LoadingDlg * _loadingDialog;
    bool _send_pending;
private slots:
    void slot_item_clicked(QListWidgetItem *item);
    void slot_user_search(std::shared_ptr<SearchInfo> si);
signals:
    void sig_jump_chat_item(std::shared_ptr<SearchInfo> si);

};

#endif // SEARCHLIST_H
