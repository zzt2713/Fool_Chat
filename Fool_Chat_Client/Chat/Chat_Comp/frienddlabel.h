#ifndef FRIENDDLABEL_H
#define FRIENDDLABEL_H

#include <QFrame>

namespace Ui {
class FrienddLabel;
}

class FrienddLabel : public QFrame
{
    Q_OBJECT

public:
    explicit FrienddLabel(QWidget *parent = nullptr);
    ~FrienddLabel();
    void SetText(QString);
    int Width();
    int Height();
    QString Text();

private:
    Ui::FrienddLabel *ui;
    QString _text;
    int _width;
    int _height;

private slots:
    void slot_close();

signals:
    void sig_close(QString);

};

#endif // FRIENDDLABEL_H
