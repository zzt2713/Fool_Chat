#ifndef NEWSTAR_H
#define NEWSTAR_H
/******************************************************************************
*
* @file       newstar.h
* @brief      发布动态页面
*
* @author     Fool
* @date       2026/05/09
* @history
*****************************************************************************/
#include "ElaDialog.h"
#include <QList>
#include <QPixmap>

namespace Ui {
class NewStar;
}

class NewStar : public ElaDialog
{
    Q_OBJECT

public:
    explicit NewStar(QWidget *parent = nullptr);
    ~NewStar();

signals:
    void sigPublish(const QString& text, const QList<QPixmap>& images);

private slots:
    void on_pushButton_clicked();
    void on_add_pic_clicked();
    void on_plainTextEdit_textChanged();

private:
    Ui::NewStar *ui;
    QList<QPixmap> _imageList;

    void ShowPicWid(bool b_ok);
    void updatePicList();
};

#endif // NEWSTAR_H
