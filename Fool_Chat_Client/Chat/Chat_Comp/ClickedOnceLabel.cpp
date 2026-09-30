#include "ClickedOnceLabel.h"

ClickedOnceLabel::ClickedOnceLabel(QWidget *parent):ElaText(parent)
{
    setCursor(Qt::PointingHandCursor);

}

ClickedOnceLabel::~ClickedOnceLabel()
{

}

void ClickedOnceLabel::mouseReleaseEvent(QMouseEvent *ev)
{
    if(ev->button() == Qt::LeftButton){
        emit clicked(this->text());
        return ;
    }

    ElaText::mouseReleaseEvent(ev);
}
