#include "regsterdialog.h"
#include "ui_regsterdialog.h"
#include "../core/httpmgr.h"
#include <QRegularExpression>
#include <QLineEdit>
#include "../core/dbmanager.h"
#include "../widgets/floatingtip.h"
#include "../widgets/msgtip.h"
#include <QShortcut>

RegsterDialog::RegsterDialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RegsterDialog),_countdown(10)
{
    ui->setupUi(this);
    ui->errTip->setProperty("state","normal");
    // 基类已是 QWidget，QDialog 的 setDefault 回车机制无效，改用快捷键（主键盘+小键盘回车）
    for (int keyId : {int(Qt::Key_Return), int(Qt::Key_Enter)}) {
        auto *sc = new QShortcut(QKeySequence(keyId), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this]() { ui->sure_btn->click(); });
    }
    ui->varEdit->setCodeLength(4);
    ui->varEdit->setInputMode(ElaCaptcha::AlphaNumeric);

    repolish(ui->errTip);
    connect(HttpMgr::GetInstance().get(),&HttpMgr::sig_reg_mod_finish,
            this,&RegsterDialog::slot_reg_mod_finish);
    initHttpHandlers();
    SetPasswdEye();
    ui->getCode->setEnabled(false);
    ui->errTip->clear();
    // 远程 DB 校验合并到 300ms 单发防抖，避免连续失焦打爆查询
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

    connect(ui->passEdit, &QLineEdit::editingFinished, this, [this](){
        checkPassValid();
    });

    connect(ui->passEdit_2, &QLineEdit::editingFinished, this, [this](){
        checkConfirmValid();
    });

    connect(ui->varEdit, &ElaCaptcha::codeCompleted, this, [this](const QString& code){
        checkVarifyValid();
    });

    _countdown_timer = new QTimer(this);
    connect(_countdown_timer, &QTimer::timeout, [this](){
        if(_countdown==0){
            _countdown_timer->stop();
            emit sigSwitchLogin();
            return;
        }
        _countdown--;
        auto str = QString("注册成功，%1s后返回登录").arg(_countdown);
        ui->tip_lb->setText(str);
    });

}

void RegsterDialog::AddTipErr(TipErr te, QString tips)
{
    _tip_errs[te] = tips;
    ui->errTip->setText(tips);
    ui->errTip->setProperty("state", "err");
    repolish(ui->errTip);
}

void RegsterDialog::DelTipErr(TipErr te)
{
    _tip_errs.remove(te);
    if (_tip_errs.empty()) {
        ui->errTip->clear();
        return;
    }
    ui->errTip->setText(_tip_errs.first());
}

void RegsterDialog::ChangeTipPage()
{
    _countdown_timer->stop();
    ui->msg->setText(ui->userEdit->text());
    ui->msg2->setText(ui->passEdit->text());

    ui->stackedWidget->setCurrentWidget(ui->page_2);
    _countdown_timer->start(1000);
}

bool RegsterDialog::checkUserValid()
{
    if(ui->userEdit->text() == ""){
        AddTipErr(TipErr::TIP_USER_ERR, tr("用户名不能为空"));
        return false;
    }
    auto db = DBManager::GetInstance();

    if (db->isUsernameExist(ui->userEdit->text())) {
        AddTipErr(TipErr::TIP_USER_EXIST, db->getLastError());
        return false;
    }
    DelTipErr(TipErr::TIP_USER_EXIST);
    DelTipErr(TipErr::TIP_USER_ERR);
    return true;
}

bool RegsterDialog::checkEmailValid()
{
    auto email = ui->emailEdit->text();
    bool match = EMAIL_RULE.match(email).hasMatch();
    if(!match){
        AddTipErr(TipErr::TIP_EMAIL_ERR, tr("邮箱地址不正确"));
        return false;
    }
    auto db = DBManager::GetInstance();

    if (db->isEmailExist(ui->emailEdit->text())) {
        AddTipErr(TipErr::TIP_EMAIL_EXIST, db->getLastError());
        return false;
    }

    DelTipErr(TipErr::TIP_EMAIL_EXIST);
    DelTipErr(TipErr::TIP_EMAIL_ERR);
    return true;
}

bool RegsterDialog::checkPassValid()
{
    auto pwd = ui->passEdit->text();
    if(pwd.length() < 6 || pwd.length()>15){
        //提示长度不准确
        AddTipErr(TipErr::TIP_PWD_ERR, tr("密码长度应为6~15"));
        return false;
    }

    // 检测是否包含非法字符
    QRegularExpression regExp(PASSWORD_RULE);
    bool match = regExp.match(pwd).hasMatch();
    if(!match){
        AddTipErr(TipErr::TIP_PWD_ERR, tr("不能包含非法字符"));
        return false;
    }

    DelTipErr(TipErr::TIP_PWD_ERR);
    return true;
}

bool RegsterDialog::checkVarifyValid()
{
    auto pass = ui->varEdit->getCode();
    if(pass.isEmpty()){
        AddTipErr(TipErr::TIP_VARIFY_ERR, tr("验证码不能为空"));
        return false;
    }

    DelTipErr(TipErr::TIP_VARIFY_ERR);
    return true;
}

void RegsterDialog::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 切页回来时清掉上次残留的错误提示
    _tip_errs.clear();
    ui->errTip->clear();
}

void RegsterDialog::Clear()
{
    ui->userEdit->setText("");
    ui->emailEdit->setText("");
    ui->passEdit->setText("");
    ui->varEdit->clear();
    ui->passEdit_2->setText("");
    _tip_errs.clear();
    ui->errTip->clear();
}

bool RegsterDialog::checkConfirmValid()
{
    QString password = ui->passEdit->text();
    QString confirm = ui->passEdit_2->text();

    if (confirm.isEmpty()) {
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("请再次输入密码"));
        return false;
    }

    if (password.isEmpty()) {
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("请先输入密码"));
        return false;
    }

    if (password != confirm) {
        AddTipErr(TipErr::TIP_CONFIRM_ERR, tr("两次输入的密码不一致"));
        return false;
    }

    DelTipErr(TipErr::TIP_CONFIRM_ERR);
    return true;
}

RegsterDialog::~RegsterDialog()
{
    delete ui;
}

void RegsterDialog::SetPasswdEye()
{
    auto setupPasswordEye = [this](QLineEdit* edit) {
        QAction *eyeAction = edit->addAction(
            QIcon(":/icons/eye-close.png"),
            QLineEdit::TrailingPosition
            );

        edit->setEchoMode(QLineEdit::Password);
        eyeAction->setVisible(false);
        QObject::connect(edit, &QLineEdit::textChanged, [eyeAction](const QString &text) {
            eyeAction->setVisible(!text.isEmpty());
        });
        QObject::connect(eyeAction, &QAction::triggered, [edit, eyeAction]() {
            bool isPassword = (edit->echoMode() == QLineEdit::Password);
            if (isPassword) {
                edit->setEchoMode(QLineEdit::Normal);
                eyeAction->setIcon(QIcon(":/icons/eye-open.png"));
            } else {
                edit->setEchoMode(QLineEdit::Password);
                eyeAction->setIcon(QIcon(":/icons/eye-close.png"));
            }
        });
    };

    setupPasswordEye(ui->passEdit);
    setupPasswordEye(ui->passEdit_2);
}

void RegsterDialog::showTip(QString str, bool b_ok)
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

void RegsterDialog::on_getCode_clicked()
{
    auto email = ui->emailEdit->text();

    bool m = EMAIL_RULE.match(email).hasMatch();

    if(m){
        // 禁用验证码获取，防止重复点击
        ui->getCode->setEnabled(false);
        ui->getCode->setText("发送中...");

        QJsonObject obj;
        obj["email"] = email;
        HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/get_varifycode"),
                                            obj, ReqId::ID_GET_VARIFY_CODE, Modules::REGISTERMOD);
    }else{
        // 理论上不会走到这里，因为按钮已被禁用
        showTip("邮箱地址不正确", false);
    }
}
void RegsterDialog::slot_reg_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    // 先检查是否是获取验证码的请求
    if (id == ReqId::ID_GET_VARIFY_CODE) {
        // 如果网络请求失败
        if (err != ErrorCodes::SUCCESS) {
            // 恢复按钮状态
            ui->getCode->setEnabled(true);
            ui->getCode->setText("获取验证码");
            showTip("网络请求失败！", false);
            return;
        }

        // 解析JSON
        QJsonDocument jsondc = QJsonDocument::fromJson(res.toUtf8());
        if(jsondc.isNull() || !jsondc.isObject()){
            // JSON解析失败，恢复按钮状态
            ui->getCode->setEnabled(true);
            ui->getCode->setText("获取验证码");
            showTip("json解析失败", false);
            return;
        }

        // 调用对应的处理器
        _handlers[id](jsondc.object());
        return;
    }

    // 处理其他请求类型的网络错误
    if(err != ErrorCodes::SUCCESS){
        showTip("网络请求失败！", false);
        return;
    }

    // 解析其他请求的JSON
    QJsonDocument jsondc = QJsonDocument::fromJson(res.toUtf8());
    if(jsondc.isNull() || !jsondc.isObject()){
        showTip("json解析失败", false);
        return;
    }

    // 调用对应的处理器
    _handlers[id](jsondc.object());
}


void RegsterDialog::initHttpHandlers()
{
    // 获取验证码回包逻辑
    _handlers.insert(ReqId::ID_GET_VARIFY_CODE,[this](const QJsonObject& jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip("参数错误！",false);
            return ;
        }

        auto email = jsonObj["email"].toString();
        showTip("验证码已发送至邮箱，请注意查收！",true);
    });

    _handlers.insert(ReqId::ID_REG_USER, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCodes::SUCCESS){
            showTip(tr("用户或邮箱已存在！"),false);
            return;
        }
        auto email = jsonObj["email"].toString();
        showTip(tr("用户注册成功！"), true);
        ChangeTipPage();
    });
}

void RegsterDialog::on_sure_btn_clicked()
{
    // 提交前与失焦校验共用同一套全套规则，任一不过即拦下
    if (!checkUserValid()) {
        return;
    }
    if (!checkEmailValid()) {
        return;
    }
    if (!checkPassValid()) {
        return;
    }
    if (!checkConfirmValid()) {
        return;
    }
    if (!checkVarifyValid()) {
        return;
    }
    QJsonObject json_obj;
    json_obj["user"] = ui->userEdit->text();
    json_obj["email"] = ui->emailEdit->text();
    json_obj["passwd"] = hashString(ui->passEdit->text());
    json_obj["confirm"] = hashString(ui->passEdit_2->text());
    json_obj["varifycode"] = ui->varEdit->getCode();
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix+"/user_register"),
                                        json_obj, ReqId::ID_REG_USER,Modules::REGISTERMOD);
}


void RegsterDialog::on_returnBtn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}


void RegsterDialog::on_cencel_btn_clicked()
{
    _countdown_timer->stop();
    emit sigSwitchLogin();
}

