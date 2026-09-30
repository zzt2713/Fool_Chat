#include "adduseritem.h"
#include "ui_adduseritem.h"


AddUserItem::AddUserItem(QWidget *parent):ListItemBase(parent),ui(new Ui::AddUserItem)
{
    ui->setupUi(this);
    SetItemType(ListItemType::ADD_USER_TIP_ITEM);
    ui->add_tip->setElaIcon(ElaIconType::UserMagnifyingGlass);
    ui->right_tip->setElaIcon(ElaIconType::AngleRight);
}

AddUserItem::~AddUserItem()
{
    delete ui;
}
