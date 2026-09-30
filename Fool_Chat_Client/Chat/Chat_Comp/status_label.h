#ifndef STATUS_LABEL_H
#define STATUS_LABEL_H
#include "ElaText.h"

class Status_label:public ElaText
{
public:
    explicit Status_label(QWidget* parent = nullptr);
    ~Status_label();
    void setStatus(bool b_online);
private:
    bool _b_online;

};

#endif // STATUS_LABEL_H
