#include "resetdialog.h"
#include "ui_resetdialog.h"
#include <QDebug>
#include <QRegularExpression>
#include "../core/httpmgr.h"
#include "../core/dbmanager.h"
#include "../widgets/msgtip.h"
#include "../widgets/floatingtip.h"
#include "ElaCaptcha.h"
#include <QShortcut>

ResetDialog::ResetDialog(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ResetDialog)
{
    ui->setupUi(this);
    ui->getCode->setEnabled(false);
    repolish(ui->errTip);
    SetPasswdEye();
    ui->varifyEdit->setCodeLength(4);
    ui->varifyEdit->setInputMode(ElaCaptcha::AlphaNumeric);

    // 基类已是 QWidget，QDialog 的 setDefault 回车机制无效，改用快捷键（主键盘+小键盘回车）
    for (int keyId : {int(Qt::Key_Return), int(Qt::Key_Enter)}) {
        auto *sc = new QShortcut(QKeySequence(keyId), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this]() { ui->sure_btn->click(); });
    }

    // 远程 DB 校验合并到 300ms 单发防抖
    _dbTimer = new QTimer(this);
    _dbTimer->setSingleShot(true);
    _dbTimer->setInterval(300);
    connect(_dbTimer, &QTimer::timeout, this, [this]() {
        int pending = _pendingDb;
        _pendingDb = 0;
        if (pending & 1) {
            checkUserValid();
        }
        if (pending & 2) {
            bool ok = checkEmailValid();
            ui->getCode->setEnabled(ok);
        }
    });

    connect(ui->userEdit, &QLineEdit::editingFinished, this, [this]() {
        _pendingDb |= 1;
        _dbTimer->start();
    });

    connect(ui->emailEdit, &QLineEdit::editingFinished, this, [this]() {
        _pendingDb |= 2;
        _dbTimer->start();
    });

    connect(ui->newPassEdit, &QLineEdit::editingFinished, this, [this](){
        checkPassValid();
    });

    connect(ui->varifyEdit, &ElaCaptcha::codeCompleted, this, [this](const QString& code){
        checkVarifyValid();
    });

    //连接reset相关信号和注册处理回调
    initHandlers();
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_reset_mod_finish, this,
            &ResetDialog::slot_reset_mod_finish);
}

ResetDialog::~ResetDialog()
{
    delete ui;
}

void ResetDialog::on_return_btn_clicked()
{
    emit switchLogin();
}

void ResetDialog::on_getCode_clicked()
{
    auto email = ui->emailEdit->text();


    auto bcheck = checkEmailValid();
    if(!bcheck){
        return;
    }
    ui->getCode->setEnabled(false);
    ui->getCode->setText("发送中...");
    //发送http请求获取验证码
    QJsonObject json_obj;
    json_obj["email"] = email;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/get_varifycode"),
                                        json_obj, ReqId::ID_GET_VARIFY_CODE,Modules::RESETMOD);
}

void ResetDialog::slot_reset_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if(err != ErrorCodes::SUCCESS){
        showTip(tr("网络请求错误"),false);
        ui->getCode->stopCountdown();
        return;
    }

    // 解析 JSON 字符串,res需转化为QByteArray
    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    //json解析错误
    if(jsonDoc.isNull()){
        showTip(tr("json解析错误"),false);
        return;
    }

    //json解析错误
    if(!jsonDoc.isObject()){
        showTip(tr("json解析错误"),false);
        return;
    }
    //调用对应的逻辑,根据id回调。
    _handlers[id](jsonDoc.object());

    return;
}

bool ResetDialog::checkUserValid()
{
    if(ui->userEdit->text() == ""){
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    auto db = DBManager::GetInstance();
    if(!(db->isUsernameExist(ui->userEdit->text()))){
        AddTipErr(TipErr::TIP_USER_NOT_EXIST, tr("用户名不存在"));
        return false;
    }

    DelTipErr(TipErr::TIP_USER_NOT_EXIST);
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}


bool ResetDialog::checkPassValid()
{
    auto pass = ui->newPassEdit->text();

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

void ResetDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 切页回来时清掉上次残留的错误提示
    _tip_errs.clear();
    ui->errTip->clear();
}

void ResetDialog::Clear()
{
    ui->userEdit->setText("");
    ui->emailEdit->setText("");
    ui->newPassEdit->setText("");
    ui->varifyEdit->clear();
    _tip_errs.clear();
    ui->errTip->clear();
}

bool ResetDialog::checkEmailValid()
{
    //验证邮箱的地址正则表达式
    auto email = ui->emailEdit->text();
    auto user = ui->userEdit->text();

    if(email == ""){
        //提示邮箱不正确
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不能为空"));
        return false;
    }
    bool match = EMAIL_RULE.match(email).hasMatch();
    if(!match){
        //提示邮箱不正确
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }

    auto db = DBManager::GetInstance();
    if(!(db->isEmailExist(ui->emailEdit->text()))){
        AddTipErr(TipErr::TIP_EMAIL_NOT_EXIST, tr("邮箱不存在"));
        return false;
    }

    if(db->isUsernameExist(user) && !db->findEmailByUsername(user,email)){
        AddTipErr(TipErr::TIP_EMAIL_NOT_EXIST, db->getLastError());
        return false;
    }

    DelTipErr(TipErr::TIP_EMAIL_NOT_EXIST);
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool ResetDialog::checkVarifyValid()
{
    auto pass = ui->varifyEdit->getCode();
    if(pass.isEmpty()){
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

void ResetDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    ui->errTip->setText(tips);
    ui->errTip->setProperty("state", "err");
    repolish(ui->errTip);
}

void ResetDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if (_tip_errs.empty()) {
        ui->errTip->clear();
        return;
    }
    ui->errTip->setText(_tip_errs.first());
}

void ResetDialog::initHandlers()
{
    //注册获取验证码回包逻辑
    _handlers.insert(ReqId::ID_GET_VARIFY_CODE, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }
        showTip("验证码已发送至邮箱，请注意查收！",true);
    });

    // 修改密码回包逻辑
    _handlers.insert(ReqId::ID_RESET_PWD, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("参数错误"),false);
            return;
        }
        showTip(tr("重置成功,点击返回登录"), true);
    });
}

void ResetDialog::showTip(QString str, bool b_ok)
{
    ADDMSG(ElaMessageBarType::Top,str,this,b_ok);
    // ShowTip(this,str,b_ok);
    // if(!b_ok){
    //     ERR(ElaMessageBarType::TopLeft,str,this);
    //     return;
    // }
    // SUCCESS(ElaMessageBarType::TopLeft,str,this);
    // if(b_ok){
    //     ui->errTip->setProperty("state","normal");
    // }else{
    //     ui->errTip->setProperty("state","err");
    // }

    // ui->errTip->setText(str);
    // repolish(ui->errTip);
}

void ResetDialog::on_sure_btn_clicked()
{
    bool valid = checkUserValid();
    if(!valid){
        return;
    }

    valid = checkEmailValid();
    if(!valid){
        return;
    }

    valid = checkPassValid();
    if(!valid){
        return;
    }

    valid = checkVarifyValid();
    if(!valid){
        return;
    }

    //发送http重置用户请求
    QJsonObject json_obj;
    json_obj["user"] = ui->userEdit->text();
    json_obj["email"] = ui->emailEdit->text();
    json_obj["passwd"] = hashString(ui->newPassEdit->text());
    json_obj["varifycode"] = ui->varifyEdit->getCode();
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/reset_pwd"),
                                        json_obj, ReqId::ID_RESET_PWD,Modules::RESETMOD);
}

void ResetDialog::SetPasswdEye()
{
    QAction *eyeAction = ui->newPassEdit->addAction(
        QIcon(":/icons/eye-close.png"),
        QLineEdit::TrailingPosition
        );

    ui->newPassEdit->setEchoMode(QLineEdit::Password);
    eyeAction->setVisible(false);

    connect(ui->newPassEdit, &QLineEdit::textChanged, [=](const QString &text) {
        eyeAction->setVisible(!text.isEmpty());
    });

    connect(eyeAction, &QAction::triggered, [=]() {
        bool isPassword = (ui->newPassEdit->echoMode() == QLineEdit::Password);

        if (isPassword) {
            ui->newPassEdit->setEchoMode(QLineEdit::Normal);
            eyeAction->setIcon(QIcon(":/icons/eye-open.png")); // 换成睁眼图标
        } else {
            ui->newPassEdit->setEchoMode(QLineEdit::Password);
            eyeAction->setIcon(QIcon(":/icons/eye-close.png")); // 换成闭眼图标
        }
    });
}

