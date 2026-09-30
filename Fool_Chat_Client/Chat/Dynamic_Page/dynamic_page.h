#ifndef DYNAMIC_PAGE_H
#define DYNAMIC_PAGE_H
/******************************************************************************
*
* @file       dynamic_page.h
* @brief      动态页面 - 展示动态列表，支持发布新动态和查看详情
*
******************************************************************************/
#include <QWidget>
#include <QList>
#include "newstar.h"
#include "dynamic_card.h"
#include "../Chat_Comp/page_base.h"

class QShowEvent;

namespace Ui {
class Dynamic_Page;
}

class Dynamic_Page : public Page_Base
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit Dynamic_Page(QWidget *parent = nullptr);
    ~Dynamic_Page();

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::Dynamic_Page *ui;
    NewStar* _new_star;
    QList<DynamicItem> _dynamicList;  // 动态数据列表
    bool _refreshPending{false};      // 点了刷新按钮等回包（收到列表才提示成功）

    // 向瀑布流追加一条动态卡片
    void addDynamicCard(const DynamicItem& item);
    // 从服务器加载动态列表
    void loadDynamicList();
};

#endif // DYNAMIC_PAGE_H
