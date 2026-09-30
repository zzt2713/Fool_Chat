#ifndef CHATTEXT_H
#define CHATTEXT_H
/******************************************************************************
*
* @file       chattext.h
* @brief      状态栏文字 Function
*
* @author     Fool
* @date       2026/03/03
* @code       SET_STATUS(str)设置状态栏
* @history
*****************************************************************************/
#include "ElaText.h"

#define SET_STATUS(s)  ChatText::instance()->setTxt(s)
#define STAUTUS() ChatText::instance()

class ChatText : public ElaText
{
private:
    static ChatText* _instance;

    ChatText(QWidget* parent = nullptr);

public:
    static ChatText* instance();
    ~ChatText();
    void setTxt(QString);

    // 防止拷贝和赋值
    ChatText(const ChatText&) = delete;
    ChatText& operator=(const ChatText&) = delete;
};

#endif // CHATTEXT_H
