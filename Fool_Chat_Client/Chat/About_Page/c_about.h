#ifndef C_ABOUT_H
#define C_ABOUT_H

#include "ElaDialog.h"

namespace Ui {
class C_About;
}

class C_About : public ElaDialog
{
    Q_OBJECT

public:
    explicit C_About(QWidget *parent = nullptr);
    ~C_About();

private:
    Ui::C_About *ui;
};

#endif // C_ABOUT_H
