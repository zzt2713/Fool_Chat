#ifndef CLIKEDBTN_H
#define CLIKEDBTN_H

#include "ElaPushButton.h"
class ClikedBtn :public ElaPushButton
{

public:
    explicit ClikedBtn(QWidget* parent = nullptr);
    ~ClikedBtn();
};

#endif // CLIKEDBTN_H
