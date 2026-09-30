#include "chattext.h"

ChatText* ChatText::_instance = nullptr;

ChatText::ChatText(QWidget* parent) : ElaText(parent)
{
    setTextPixelSize(14);
    setMaximumWidth(500);
    setMinimumWidth(200);
}

ChatText::~ChatText()
{
    if (_instance == this) {
        _instance = nullptr;
    }
}

void ChatText::setTxt(QString str)
{
    setText(str+"\t");
}

ChatText* ChatText::instance()
{
    if (_instance == nullptr) {
        _instance = new ChatText();
    }
    return _instance;
}
