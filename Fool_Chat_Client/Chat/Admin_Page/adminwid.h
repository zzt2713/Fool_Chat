#ifndef ADMINWID_H
#define ADMINWID_H

#include <QWidget>

namespace Ui {
class AdminWid;
}

class AdminWid : public QWidget
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit AdminWid(QWidget *parent = nullptr);
    ~AdminWid();

private:
    Ui::AdminWid *ui;
};

#endif // ADMINWID_H
