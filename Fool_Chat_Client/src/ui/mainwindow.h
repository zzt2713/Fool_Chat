#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include "login.h"
#include "regsterdialog.h"
#include "resetdialog.h"
#include "../../Chat/c_window.h"

/******************************************************************************
*
* @file       mainwindow.h
* @brief      主窗口
*
* @author     Fool
* @date       2026/02/01
* @history
*****************************************************************************/

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

public slots:
    void SlotSwitchReg();
    void SlotSwitchLogin();
    void SlotSwitchReset();
    void SlotSwitchChat();

private:
    Ui::MainWindow *ui;

    // 页面容器
    QStackedWidget* _mainStack;

    // 三个子界面指针
    login *_login_dlg = nullptr;
    RegsterDialog * _reg_dlg = nullptr;
    ResetDialog *_res_dlg = nullptr;

    // 聊天主界面
    C_Window *_chat_dlg = nullptr;

    int b_clear;
    bool _banner_loaded = false;

    void loadBanner();
};
#endif // MAINWINDOW_H
