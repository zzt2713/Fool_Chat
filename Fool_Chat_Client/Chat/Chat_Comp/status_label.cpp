#include "status_label.h"

Status_label::Status_label(QWidget *parent):ElaText(parent),_b_online(false)
{
    setFixedSize(20,20);
}

Status_label::~Status_label()
{

}

void Status_label::setStatus(bool b_online)
{
    _b_online = b_online;
    if (_b_online) {
        setStyleSheet(
            "#status_lb{"
            "background-color: #24e68a;"
            "border-radius: 6px;"
            "border: none;"
            "padding: 0px;"
            "}"
        );
        setToolTip("在线");
    } else {
        setStyleSheet(
            "#status_lb{"
            "background-color: #808080;"
            "border-radius: 6px;"
            "border: none;"
            "padding: 0px;"
            "}"
        );
        setToolTip("离线");
    }
}
