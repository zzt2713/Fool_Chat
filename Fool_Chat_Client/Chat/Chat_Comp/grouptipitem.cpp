#include "grouptipitem.h"
#include "ui_grouptipitem.h"
#include "themedtext.h"

GroupTipItem::GroupTipItem(QWidget *parent)
    : ListItemBase(parent)
    , ui(new Ui::GroupTipItem),_tip("")
{
    ui->setupUi(this);
    SetItemType(ListItemType::GROUP_TIP_ITEM);
    // 分组名是次级文字：淡灰/淡白
    BindMutedTextToTheme(ui->label);
}

GroupTipItem::~GroupTipItem()
{
    delete ui;
}

QSize GroupTipItem::sizeHint() const
{
    return QSize(250,25);
}

void GroupTipItem::SetGroupTip(QString str)
{
    ui->label->setText(str);
}
