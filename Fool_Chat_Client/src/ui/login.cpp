#include "login.h"
#include "ui_login.h"
#include "../core/dbmanager.h"
#include "../widgets/msgtip.h"
#include "../core/httpmgr.h"
#include <QUrl>
#include "../core/tcpmgr.h"
#include "../widgets/floatingtip.h"
#include "../core/ranimg.h"
#include "ElaLineEdit.h"
#include <QShortcut>
#include <QMenu>
#include <QSettings>
#include <QCoreApplication>
#include <QPainter>
#include <QPen>

login::login(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);
    ui->forget_label->SetState("normal","hover","","selected","selected_hover","");

    ininHttpHandlers();
    SetPasswdEye();
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(0);

    // 基类已是 QWidget，QDialog 的 setDefault 回车机制无效，改用快捷键（主键盘+小键盘回车）
    for (int keyId : {int(Qt::Key_Return), int(Qt::Key_Enter)}) {
        auto *sc = new QShortcut(QKeySequence(keyId), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this]() { ui->loginBtn->click(); });
    }

    // m_loadingMovie = new QMovie(":/icons/loadings.gif");
    // m_loadingMovie->setScaledSize(QSize(200, 200));
    // ui->lbl_loading_img->setMovie(m_loadingMovie);

    ui->stackedWidget->setCurrentIndex(0);

    // 失焦即校验，避免错误提示在修正后滞留
    connect(ui->userEdit, &QLineEdit::editingFinished, this, [this]() {
        checkUserValid();
    });
    connect(ui->passEdit, &QLineEdit::editingFinished, this, [this]() {
        checkPassValid();
    });

    m_dotCount = 0;
    m_btnTimer = new QTimer(this);
    connect(m_btnTimer, &QTimer::timeout, this, [this]() {
        m_dotCount = (m_dotCount + 1) % 4;
        QString text = "登录中";
        for (int i = 0; i < m_dotCount; ++i) {
            text += ".";
        }
        ui->loginBtn->setText(text);
    });

    m_labelDotCount = 0;
    m_labelTimer = new QTimer(this);

    connect(m_labelTimer, &QTimer::timeout, this, [this]() {
        m_labelDotCount = (m_labelDotCount + 1) % 4;
        QString text = "正在初始化";
        for (int i = 0; i < m_labelDotCount; ++i) {
            text += ".";
        }
        ui->load_login_lb->setText(text);
    });


    connect(ui->forget_label, &ClickedLabel::clicked, this, &login::slot_forget_pwd);
    connect(ui->regBtn,&QPushButton::clicked,this,&login::switchRegister);
    // 回包信号连接
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_login_mod_finish,this,&login::slot_login_mod_finish);
    // 连接tcp
    connect(this,&login::sig_connect_tcp,TcpMgr::GetInstance().get(),&TcpMgr::slot_tcp_connect);
    //连接tcp管理者发出的连接成功信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_con_success, this, &login::slot_tcp_con_failed);
    //连接tcp管理者发出的登陆失败信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_login_failed, this, &login::slot_login_failed);

    // 快速登录：回填上次登录账号，勾选过记住密码则连带密码
    loadAccounts();
    applySavedAccount();

    // 历史账号下拉收进账号输入框内（与密码眼睛按钮同一交互形态），有历史才显示
    // 按设备像素比重绘箭头并细化笔画，避免高 DPI 下发虚
    const qreal dpr = ui->userEdit->devicePixelRatioF();
    QPixmap chevron(qRound(16 * dpr), qRound(16 * dpr));
    chevron.fill(Qt::transparent);
    chevron.setDevicePixelRatio(dpr);
    QPainter painter(&chevron);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor("#909399"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(5, 6.2), QPointF(8, 9.2));
    painter.drawLine(QPointF(8, 9.2), QPointF(11, 6.2));
    painter.end();
    _historyAction = ui->userEdit->addAction(QIcon(chevron), QLineEdit::TrailingPosition);
    _historyAction->setToolTip("历史账号");
    _historyAction->setVisible(!_accountOrder.isEmpty());
    connect(_historyAction, &QAction::triggered, this, [this]() {
        if (_accountOrder.isEmpty()) {
            return;
        }
        QMenu menu(this);
        // 与账号输入框同宽同左缘，观感即输入框下拉
        menu.setFixedWidth(ui->userEdit->width());
        for (const QString& user : _accountOrder) {
            menu.addAction(user);
        }
        QAction* act = menu.exec(ui->userEdit->mapToGlobal(QPoint(0, ui->userEdit->height())));
        if (act == nullptr) {
            return;
        }
        const QString user = act->text();
        ui->userEdit->setText(user);
        const QString pwd = _savedAccounts.value(user);
        ui->rememberCheck->setChecked(!pwd.isEmpty());
        if (pwd.isEmpty()) {
            ui->passEdit->clear();
            ui->passEdit->setFocus();
            return;
        }
        ui->passEdit->setText(pwd);
    });
}

login::~login()
{
    if (m_labelTimer && m_labelTimer->isActive()) {
        m_labelTimer->stop();
    }
    delete ui;
}

void login::SetPasswdEye()
{
    QAction *eyeAction = ui->passEdit->addAction(
        QIcon(":/icons/eye-close.png"),
        QLineEdit::TrailingPosition
        );

    ui->passEdit->setEchoMode(QLineEdit::Password);
    eyeAction->setVisible(false);

    connect(ui->passEdit, &QLineEdit::textChanged, [=](const QString &text) {
        eyeAction->setVisible(!text.isEmpty());
    });

    connect(eyeAction, &QAction::triggered, [=]() {
        bool isPassword = (ui->passEdit->echoMode() == QLineEdit::Password);

        if (isPassword) {
            ui->passEdit->setEchoMode(QLineEdit::Normal);
            eyeAction->setIcon(QIcon(":/icons/eye-open.png")); // 换成睁眼图标
        } else {
            ui->passEdit->setEchoMode(QLineEdit::Password);
            eyeAction->setIcon(QIcon(":/icons/eye-close.png")); // 换成闭眼图标
        }
    });
}

void login::ininHttpHandlers()
{
    _handlers.insert(ReqId::ID_LOGIN_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("用户名或密码错误！"),false);
            enableBtn(true);
            return;
        }

        // 获取登录信息
        NAME = jsonObj["user"].toString();
        EMAIL = jsonObj["email"].toString();
        // 记住账号；勾选了记住密码才落盘密码
        saveAccount(NAME, ui->passEdit->text(), ui->rememberCheck->isChecked());

        //发送信号通知 tcpMgr发送长链接
        ServerInfo si;
        si.Uid = jsonObj["uid"].toInt();
        si.Host = jsonObj["host"].toString();
        si.Port = jsonObj["port"].toString();
        si.Token = jsonObj["token"].toString();
        _uid = si.Uid;
        _token = si.Token;

        emit sig_connect_tcp(si);
    });
}

void login::slot_forget_pwd()
{
    emit switchReset();
}

void login::loadAccounts()
{
    // 不能存 config.ini：构建产物会被 PostBuild 覆盖
    QSettings settings(QCoreApplication::applicationDirPath() + "/accounts.ini", QSettings::IniFormat);
    _lastUser = settings.value("last_user").toString();
    _savedAccounts.clear();
    _accountOrder.clear();
    int size = settings.beginReadArray("accounts");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString user = settings.value("user").toString();
        if (user.isEmpty()) {
            continue;
        }
        _accountOrder.append(user);
        _savedAccounts[user] = settings.value("pwd").toString();
    }
    settings.endArray();
}

void login::writeAccounts()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/accounts.ini", QSettings::IniFormat);
    settings.setValue("last_user", _lastUser);
    settings.remove("accounts");
    settings.beginWriteArray("accounts");
    for (int i = 0; i < _accountOrder.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("user", _accountOrder[i]);
        settings.setValue("pwd", _savedAccounts.value(_accountOrder[i]));
    }
    settings.endArray();
    if (_historyAction) {
        _historyAction->setVisible(!_accountOrder.isEmpty());
    }
}

void login::saveAccount(const QString& user, const QString& pwd, bool remember)
{
    if (user.isEmpty()) {
        return;
    }
    _lastUser = user;
    // 密码明文保存仅限学习项目使用，生产环境应加密或使用系统凭据库
    _savedAccounts[user] = remember ? pwd : QString();
    _accountOrder.removeAll(user);
    _accountOrder.prepend(user);
    while (_accountOrder.size() > 10) {
        _savedAccounts.remove(_accountOrder.takeLast());
    }
    writeAccounts();
}

void login::on_loginBtn_clicked()
{
    if(!checkUserValid()){
        return;
    }
    if(!checkPassValid()){
        return;
    }
    // if(!checkUserIsexist()){
    //     return;
    // }

    enableBtn(false);

    m_dotCount = 0;
    ui->loginBtn->setText("登录中");
    m_btnTimer->start(400);

    auto user = ui->userEdit->text();
    auto pwd = hashString(ui->passEdit->text());
    QJsonObject json_obj;
    json_obj["user"] = user;
    json_obj["passwd"] = pwd;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/user_login"),
                                        json_obj,ReqId::ID_LOGIN_USER,Modules::LOGINMOD);
}

void login::slot_login_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if (m_labelTimer && m_labelTimer->isActive()) {
        m_labelTimer->stop();
    }
    if(err != ErrorCodes::SUCCESS){
        showTip("网络请求错误！", false);
        enableBtn(true);
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());

    if(jsonDoc.isNull() || !jsonDoc.isObject()){
        showTip("json解析错误", false);
        enableBtn(true);
        return;
    }

    _handlers[id](jsonDoc.object());

}
void login::slot_login_failed(int err)
{
    if (m_labelTimer && m_labelTimer->isActive()) {
        m_labelTimer->stop();
    }
    QString result = QString("登录失败，err is %1").arg(err);
    showTip(result,false);
    enableBtn(true);
    ui->stackedWidget->setCurrentWidget(ui->page_login); // 切回输入界面
    // m_loadingMovie->stop();

}

void login::slot_tcp_con_failed(bool bsuccess)
{
    if(bsuccess){
        m_labelTimer->start(500);
        ADDMSG(ElaMessageBarType::Top,"连接成功，正在跳转....",this,bsuccess,5000);
        ui->stackedWidget->setCurrentWidget(ui->page_loading);
        // m_loadingMovie->start();

        QJsonObject obj;
        obj["uid"] = _uid;
        obj["token"] = _token;
        QJsonDocument doc(obj);

        QByteArray jsonStr = doc.toJson(QJsonDocument::Indented);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_CHAT_LOGIN,jsonStr);
    }else{
        showTip("网络异常，无法连接到聊天服务器", false);
        enableBtn(true);

        ui->stackedWidget->setCurrentWidget(ui->page_login);
        // m_loadingMovie->stop();
    }
}

void login::showTip(QString str, bool b_ok)
{
    ADDMSG(ElaMessageBarType::Top,str,this,b_ok);
    // MSGTIP(ElaMessageBarType::TopLeft,str,this,b_ok);
    // ShowTip(this, str, b_ok);
    // if(!b_ok){
    //     ERR(ElaMessageBarType::TopLeft,str,this);
    //     return;
    // }
    // SUCCESS(ElaMessageBarType::TopLeft,str,this);

    /*
    if(b_ok){
        ui->errTip->setProperty("state","normal");
    }else{
        ui->errTip->setProperty("state","err");
    }
    ui->errTip->setText(str);
    repolish(ui->errTip);
    */
}

void login::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    ui->errTip->setText(tips);
    ui->errTip->setProperty("state", "err");
    repolish(ui->errTip);
}

void login::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if (_tip_errs.empty()) {
        ui->errTip->clear();
        return;
    }
    ui->errTip->setText(_tip_errs.first());
}

void login::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 切页回来时清掉上次残留的错误提示
    _tip_errs.clear();
    ui->errTip->clear();
}

void login::Clear()
{
    ui->userEdit->setText("");
    ui->passEdit->setText("");
    applySavedAccount();
    _tip_errs.clear();
    ui->errTip->clear();
}

void login::applySavedAccount()
{
    if (_lastUser.isEmpty() || !_savedAccounts.contains(_lastUser)) {
        return;
    }
    ui->userEdit->setText(_lastUser);
    const QString pwd = _savedAccounts.value(_lastUser);
    ui->rememberCheck->setChecked(!pwd.isEmpty());
    if (!pwd.isEmpty()) {
        ui->passEdit->setText(pwd);
    }
}

bool login::enableBtn(bool enabled)
{
    ui->loginBtn->setEnabled(enabled);
    ui->regBtn->setEnabled(enabled);

    if (enabled) {
        if (m_btnTimer && m_btnTimer->isActive()) {
            m_btnTimer->stop();
        }
        ui->loginBtn->setText("登 录"); // 恢复原来的文字
    }

    return true;
}

bool login::checkUserValid()
{
    if(ui->userEdit->text() == ""){
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool login::checkUserIsexist()
{
    auto db = DBManager::GetInstance();
    if(!(db->isUsernameExist(ui->userEdit->text()))){
        AddTipErr(TipErr::TIP_USER_NOT_EXIST, db->getLastError());
        return false;
    }

    DelTipErr(TipErr::TIP_USER_NOT_EXIST);
    return true;
}

bool login::checkPassValid()
{
    auto pass = ui->passEdit->text();

    if(pass.length() < 6 || pass.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    QRegularExpression regExp(PASSWORD_RULE);
    bool match = regExp.match(pass).hasMatch();
    if(!match){
        //提示字符非法
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}
