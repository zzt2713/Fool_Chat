#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include <QWidget>
#include <QShowEvent>
#include "../core/global.h"

namespace Ui {
class ResetDialog;
}

class ResetDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();
    void Clear();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void on_return_btn_clicked();
    void on_getCode_clicked();
    void slot_reset_mod_finish(ReqId id, QString res, ErrorCodes err);
    void on_sure_btn_clicked();

private:
    bool checkUserValid();
    bool checkPassValid();

    void showTip(QString str,bool b_ok);
    bool checkEmailValid();
    bool checkVarifyValid();
    void AddTipErr(TipErr te,QString tips);
    void DelTipErr(TipErr te);
    void initHandlers();
    void SetPasswdEye();
    Ui::ResetDialog *ui;
    QMap<TipErr, QString> _tip_errs;
    QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;

    // 远程 DB 校验防抖：bit1=用户名 bit2=邮箱
    QTimer *_dbTimer;
    int _pendingDb = 0;

signals:
    void switchLogin();
};

#endif // RESETDIALOG_H
