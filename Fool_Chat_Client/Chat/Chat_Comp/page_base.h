#ifndef PAGE_BASE_H
#define PAGE_BASE_H
#include "ElaScrollPage.h"
#include <QVBoxLayout>
/******************************************************************************
*
* @file       page_base.h
* @brief      页面内容基类 Function
*
* @author     Fool
* @date       2026/03/03
* @history
*****************************************************************************/
class Page_Base:public ElaScrollPage
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit Page_Base(QWidget* parent = nullptr);
    ~Page_Base() override;
protected:
    void createCustomWidget(QString desText);

};

#endif // PAGE_BASE_H
