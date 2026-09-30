#ifndef C_SEARCHEDIT_H
#define C_SEARCHEDIT_H
/******************************************************************************
*
* @file       c_searchedit.h
* @brief      搜索框 Function
*
* @author     Fool
* @date       2026/03/08
* @history
*****************************************************************************/
#include "ElaLineEdit.h"

class C_SearchEdit :public ElaLineEdit
{
    Q_OBJECT
public:
    explicit C_SearchEdit(QWidget* parent = nullptr);
    ~C_SearchEdit() override;

};

#endif // C_SEARCHEDIT_H
