#include "c_searchedit.h"
#include <QAction>

C_SearchEdit::C_SearchEdit(QWidget *parent):ElaLineEdit(parent)
{
    this->setMaxLength(15);

    this->setFocusPolicy(Qt::StrongFocus);
    QAction *searchAction = new QAction(this);
    searchAction->setIcon(QIcon(":/icons/search.png"));
    this->addAction(searchAction, QLineEdit::LeadingPosition);
}

C_SearchEdit::~C_SearchEdit()
{

}
