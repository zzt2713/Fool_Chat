#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QMouseEvent>
#include <QWindow>
#include "../core/tcpmgr.h"
#include "../widgets/msgtip.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),b_clear(0)
{
    ui->setupUi(this);
    // 无边框 + 背景透明，圆角外的区域透出桌面
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle("Fool Chat");

    connect(ui->wndMinBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(ui->wndCloseBtn, &QPushButton::clicked, this, &MainWindow::close);

    _mainStack = ui->mainStack;

    _login_dlg = new login(this);
    _reg_dlg = new RegsterDialog(this);
    _res_dlg = new ResetDialog(this);

    _mainStack->addWidget(_login_dlg);
    _mainStack->addWidget(_reg_dlg);
    _mainStack->addWidget(_res_dlg);

    connect(_login_dlg, &login::switchRegister, this, &MainWindow::SlotSwitchReg);
    connect(_login_dlg, &login::switchReset, this, &MainWindow::SlotSwitchReset);
    connect(_reg_dlg, &RegsterDialog::sigSwitchLogin, this, &MainWindow::SlotSwitchLogin);
    connect(_res_dlg, &ResetDialog::switchLogin, this, &MainWindow::SlotSwitchLogin);
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_switch_chatdlg, this, &MainWindow::SlotSwitchChat);

    // 被顶号/踢下线：销毁聊天窗口回登录页
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_kicked, this, [this]() {
        if (_chat_dlg) {
            _chat_dlg->close();
            delete _chat_dlg;
            _chat_dlg = nullptr;
        }
        SlotSwitchLogin();
        showNormal();
        ADDMSG(ElaMessageBarType::Top, "账号在其他设备登录，请重新登录", this, 0, 3000);
    });

    _mainStack->setCurrentWidget(_login_dlg);
}

MainWindow::~MainWindow()
{
    if (_chat_dlg) {
        delete _chat_dlg;
        _chat_dlg = nullptr;
    }
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 自由子控件不归布局管，布局激活后重新定位并置顶，避免被面板盖住
    ui->wndMinBtn->setGeometry(580, 6, 34, 28);
    ui->wndCloseBtn->setGeometry(618, 6, 34, 28);
    ui->wndMinBtn->raise();
    ui->wndCloseBtn->raise();
    if (!_banner_loaded) {
        loadBanner();
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // 无边框窗口没有系统标题栏，空白区域按住即拖动
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::loadBanner()
{
    QPixmap src(":/images/banner_3.jpg");
    if (src.isNull()) {
        return;
    }
    QSize target = ui->bg_image->size();
    if (target.width() <= 0 || target.height() <= 0) {
        target = ui->leftPanel->size();
    }
    if (target.width() <= 0 || target.height() <= 0) {
        target = QSize(260, 500);
    }

    QPixmap scaled = src.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap bg(target);
    bg.fill(Qt::transparent);

    QPainter painter(&bg);
    painter.setRenderHint(QPainter::Antialiasing);
    // 只圆左侧两角，右侧保持直角与白色面板拼接
    const qreal r = 12;
    QPainterPath rounded;
    rounded.addRoundedRect(QRectF(bg.rect()), r, r);
    QPainterPath squareRight;
    squareRight.addRect(QRectF(r, 0, target.width() - r, target.height()));
    painter.setClipPath(rounded.united(squareRight));

    const int x = (scaled.width() - target.width()) / 2;
    const int y = (scaled.height() - target.height()) / 2;
    painter.drawPixmap(-x, -y, scaled);
    painter.fillRect(bg.rect(), QColor(0, 0, 0, 100));
    ui->bg_image->setPixmap(bg);
    _banner_loaded = true;
}

void MainWindow::SlotSwitchReg()
{
    setWindowTitle("注册账号");
    ui->cardTitle->setText("注册账号");
    _mainStack->setCurrentWidget(_reg_dlg);
    if(b_clear>2){
        _reg_dlg->Clear();
    }

    ++ b_clear;
}

void MainWindow::SlotSwitchLogin()
{
    setWindowTitle("FoolChat");
    ui->cardTitle->setText("登录");
    _mainStack->setCurrentWidget(_login_dlg);
    if(b_clear>2){
        _login_dlg->Clear();
    }
    ++ b_clear;
}

void MainWindow::SlotSwitchReset()
{
    setWindowTitle("找回密码");
    ui->cardTitle->setText("找回密码");
    _mainStack->setCurrentWidget(_res_dlg);
    if(b_clear>2){
        _res_dlg->Clear();
    }

    ++ b_clear;
}

void MainWindow::SlotSwitchChat()
{
    // 重连重登不重建聊天窗口（踢下线流程会先销毁 _chat_dlg）
    if (_chat_dlg) {
        _chat_dlg->show();
        return;
    }
    _chat_dlg = new C_Window();

    // 退出登录：断线（禁止重连）→ 关聊天窗 → 回登录页
    connect(_chat_dlg, &C_Window::sigLogoutRequested, this, [this]() {
        TcpMgr::GetInstance()->logout();
        if (_chat_dlg) {
            _chat_dlg->close();
            delete _chat_dlg;
            _chat_dlg = nullptr;
        }
        SlotSwitchLogin();
        showNormal();
    });

    // 设置聊天窗口属性
    _chat_dlg->setWindowFlags(Qt::Window);
    _chat_dlg->setWindowTitle("Fool Chat");
    _chat_dlg->show();

    // 隐藏主登录窗口
    this->hide();

    // // 释放登录相关的资源，节省内存
    // if(_login_dlg) { delete _login_dlg; _login_dlg = nullptr; }
    // if(_reg_dlg)   { delete _reg_dlg;   _reg_dlg = nullptr; }
    // if(_res_dlg)   { delete _res_dlg;   _res_dlg = nullptr; }
}
