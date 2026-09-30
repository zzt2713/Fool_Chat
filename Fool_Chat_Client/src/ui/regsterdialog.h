#ifndef REGSTERDIALOG_H
#define REGSTERDIALOG_H

#include "../core/global.h"
#include <QShowEvent>
/******************************************************************************
*
* @file       regsterdialog.h
* @brief      注册界面
*
* @author     Fool
* @date       2026/02/03
* @history
*****************************************************************************/
namespace Ui {
class RegsterDialog;
}

class RegsterDialog : public QWidget
{
    Q_OBJECT

public:
    explicit RegsterDialog(QWidget *parent = nullptr);
    ~RegsterDialog();

    void Clear();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void on_getCode_clicked();
    void slot_reg_mod_finish(ReqId id,QString res,ErrorCodes err);
    void on_sure_btn_clicked();
    void on_returnBtn_clicked();
    void on_cencel_btn_clicked();

private:
    Ui::RegsterDialog *ui;
    QMap<ReqId,std::function<void(const QJsonObject&)>> _handlers;
    QMap<TipErr, QString> _tip_errs;

    QTimer *_countdown_timer;
    int _countdown;

    // 远程 DB 校验防抖：bit1=用户名 bit2=邮箱
    QTimer *_dbTimer;
    int _pendingDb = 0;

    bool checkUserValid();
    bool checkEmailValid();
    bool checkPassValid();
    bool checkConfirmValid();
    bool checkVarifyValid();

    void SetPasswdEye();
    void showTip(QString str,bool b_ok);
    void initHttpHandlers();
    void AddTipErr(TipErr te, QString tips);
    void DelTipErr(TipErr te);
    void ChangeTipPage();

signals:
    void sigSwitchLogin();
};

#endif // REGSTERDIALOG_H
