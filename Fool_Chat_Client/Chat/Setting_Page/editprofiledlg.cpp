#include "editprofiledlg.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaPersonPicture.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaWindow.h"
#include "httpmgr.h"
#include "msgtip.h"
#include "usermgr.h"
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QShortcut>

EditProfileDlg::EditProfileDlg(QWidget* parent)
    : ElaDialog(parent)
{
    setWindowTitle("编辑个人资料");
    setFixedSize(440, 530);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    QString cur_icon = user_info ? user_info->_icon : "";

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(48, 24, 48, 24);
    mainLayout->setSpacing(16);

    // 头像预览
    QHBoxLayout* avatarRow = new QHBoxLayout();
    _avatarPreview = new ElaPersonPicture(this);
    _avatarPreview->setPictureSize(88);
    avatarRow->addStretch();
    avatarRow->addWidget(_avatarPreview);
    avatarRow->addStretch();
    mainLayout->addLayout(avatarRow);

    // 头像候选：5 张预置图，当前头像不在预置内时追加为候选
    QStringList candidates;
    for (const auto& path : head) {
        candidates << path;
    }
    if (!cur_icon.isEmpty() && !candidates.contains(cur_icon)) {
        candidates << cur_icon;
    }
    _selectedIcon = (!cur_icon.isEmpty() && candidates.contains(cur_icon)) ? cur_icon : head.front();
    _avatarPreview->setPicture(QPixmap(_selectedIcon));

    QHBoxLayout* iconRow = new QHBoxLayout();
    iconRow->setSpacing(12);
    iconRow->addStretch();
    for (const QString& path : candidates) {
        ElaPushButton* iconBtn = new ElaPushButton(this);
        iconBtn->setFixedSize(52, 52);
        iconBtn->setIcon(QIcon(path));
        iconBtn->setIconSize(QSize(40, 40));
        connect(iconBtn, &ElaPushButton::clicked, this, [this, path, iconBtn]() {
            _selectedIcon = path;
            _avatarPreview->setPicture(QPixmap(path));
            // ElaPersonPicture 的属性 setter 不触发重绘，需手动刷新
            _avatarPreview->repaint();
            applyIconSelection(iconBtn);
        });
        iconRow->addWidget(iconBtn);
        _iconBtns.append(iconBtn);
    }
    iconRow->addStretch();
    mainLayout->addLayout(iconRow);
    int selIdx = candidates.indexOf(_selectedIcon);
    if (selIdx >= 0 && selIdx < _iconBtns.size()) {
        applyIconSelection(_iconBtns[selIdx]);
    }

    // 表单行：左侧定宽标签，右侧控件左对齐成列
    auto makeLabel = [this](const QString& text) {
        ElaText* label = new ElaText(text, this);
        label->setWordWrap(false);
        label->setTextPixelSize(15);
        label->setFixedWidth(44);
        return label;
    };

    // 昵称
    QHBoxLayout* nickRow = new QHBoxLayout();
    nickRow->setSpacing(12);
    nickRow->addWidget(makeLabel("昵称"));
    _nickEdit = new ElaLineEdit(this);
    _nickEdit->setPlaceholderText("请输入昵称");
    _nickEdit->setText(user_info ? user_info->_nick : "");
    nickRow->addWidget(_nickEdit, 1);
    mainLayout->addLayout(nickRow);

    // 个性签名
    QHBoxLayout* descRow = new QHBoxLayout();
    descRow->setSpacing(12);
    descRow->addWidget(makeLabel("签名"));
    _descEdit = new ElaLineEdit(this);
    _descEdit->setPlaceholderText("介绍一下自己");
    _descEdit->setText(user_info ? user_info->_desc : "");
    descRow->addWidget(_descEdit, 1);
    mainLayout->addLayout(descRow);

    // 性别
    QHBoxLayout* sexRow = new QHBoxLayout();
    sexRow->setSpacing(12);
    sexRow->addWidget(makeLabel("性别"));
    _sexComboBox = new ElaComboBox(this);
    _sexComboBox->addItem("保密");
    _sexComboBox->addItem("男");
    _sexComboBox->addItem("女");
    int sex = user_info ? user_info->_sex : 0;
    _sexComboBox->setCurrentIndex(sex >= 0 && sex <= 2 ? sex : 0);
    sexRow->addWidget(_sexComboBox);
    sexRow->addStretch();
    mainLayout->addLayout(sexRow);

    // UID（只读）
    QHBoxLayout* uidRow = new QHBoxLayout();
    uidRow->setSpacing(12);
    uidRow->addWidget(makeLabel("UID"));
    ElaText* uidValue = new ElaText(QString::number(UserMgr::GetInstance()->GetUid()), this);
    uidValue->setWordWrap(false);
    uidValue->setTextPixelSize(15);
    uidRow->addWidget(uidValue);
    uidRow->addStretch();
    mainLayout->addLayout(uidRow);

    // 保存
    ElaPushButton* saveBtn = new ElaPushButton("保存", this);
    saveBtn->setFixedHeight(40);
    connect(saveBtn, &ElaPushButton::clicked, this, &EditProfileDlg::saveProfile);
    mainLayout->addStretch();
    mainLayout->addWidget(saveBtn);

    initHandlers();
    connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_setting_mod_finish,
            this, &EditProfileDlg::slot_setting_mod_finish);

    // 回车保存（主键盘+小键盘），Esc 关闭走 QDialog 默认
    for (int keyId : {int(Qt::Key_Return), int(Qt::Key_Enter)}) {
        auto* sc = new QShortcut(QKeySequence(keyId), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, &EditProfileDlg::saveProfile);
    }
    moveToCenter();
}

EditProfileDlg::~EditProfileDlg()
{
}

void EditProfileDlg::applyIconSelection(ElaPushButton* btn)
{
    for (ElaPushButton* b : _iconBtns) {
        b->setGraphicsEffect(nullptr);
    }
    auto* glow = new QGraphicsDropShadowEffect(btn);
    glow->setColor(QColor(64, 158, 255));
    glow->setBlurRadius(18);
    glow->setOffset(0);
    btn->setGraphicsEffect(glow);
}

void EditProfileDlg::initHandlers()
{
    // HTTP 回包原样带回请求 ID，处理表按请求 ID 注册
    _handlers.insert(ReqId::ID_MODIFY_PROFILE_REQ, [this](QJsonObject jsonObj) {
        int error = jsonObj["error"].toInt();
        if (error != ErrorCodes::SUCCESS) {
            ADDMSG(ElaMessageBarType::Top, "资料保存失败", this, 0, 3000);
            return;
        }

        auto user_info = UserMgr::GetInstance()->GetUserInfo();
        if (user_info) {
            user_info->_nick = _nickEdit->text().trimmed();
            user_info->_desc = _descEdit->text().trimmed();
            user_info->_sex = _sexComboBox->currentIndex();
            user_info->_icon = _selectedIcon;
        }
        UserMgr::GetInstance()->setIcon(_selectedIcon);

        // 刷新主窗口左下角用户卡片（头像 + 昵称）
        if (auto* mainWnd = qobject_cast<ElaWindow*>(parentWidget())) {
            mainWnd->setUserInfoCardPixmap(QPixmap(_selectedIcon));
            if (user_info) {
                mainWnd->setUserInfoCardTitle(user_info->_nick);
            }
            mainWnd->update();
        }
        ADDMSG(ElaMessageBarType::Top, "资料已更新", parentWidget(), 1, 2000);
        close();
    });
}

void EditProfileDlg::slot_setting_mod_finish(ReqId id, QString res, ErrorCodes err)
{
    if (err != ErrorCodes::SUCCESS) {
        ADDMSG(ElaMessageBarType::Top, "网络请求错误", this, 0, 3000);
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        ADDMSG(ElaMessageBarType::Top, "回包解析错误", this, 0, 3000);
        return;
    }

    if (_handlers.contains(id)) {
        _handlers[id](jsonDoc.object());
    }
}

void EditProfileDlg::saveProfile()
{
    QString nick = _nickEdit->text().trimmed();
    if (nick.isEmpty() || nick.length() > 30) {
        ADDMSG(ElaMessageBarType::Top, "昵称需为1-30个字符", this, 0, 3000);
        return;
    }

    QString desc = _descEdit->text().trimmed();
    if (desc.length() > 60) {
        ADDMSG(ElaMessageBarType::Top, "签名最多60个字符", this, 0, 3000);
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    if (!user_info) {
        return;
    }

    QJsonObject json_obj;
    json_obj["uid"] = user_info->_uid;
    json_obj["name"] = user_info->_name;
    json_obj["nick"] = nick;
    json_obj["desc"] = desc;
    json_obj["sex"] = _sexComboBox->currentIndex();
    json_obj["icon"] = _selectedIcon;
    HttpMgr::GetInstance()->PostHttpReq(QUrl(gate_url_prefix + "/mod_profile"),
                                        json_obj, ReqId::ID_MODIFY_PROFILE_REQ, Modules::SETTINGMOD);
}
