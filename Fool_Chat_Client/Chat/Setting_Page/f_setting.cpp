#include "f_setting.h"
#include "editprofiledlg.h"
#include "ElaApplication.h"
#include "ElaComboBox.h"
#include "ElaLog.h"
#include "ElaRadioButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToggleSwitch.h"
#include "ElaWindow.h"
#include <QButtonGroup>
#include <QHBoxLayout>
#include "ElaPushButton.h"
#include <QThread>
#include "ElaToolButton.h"
#include <QSettings>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDate>
#include <QApplication>
#include <QFont>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "msgtip.h"
#include "Logger.h"
#include "usermgr.h"
#include "tcpmgr.h"
#include <QJsonDocument>
#include "ElaLineEdit.h"
#include "ElaSlider.h"
#include "aimgr.h"
#include <QPainter>

// 壁纸缓存目录（应用目录/wallpaper/）：刷新/自定义时覆盖，启动与主题切换时读取
static QString WallpaperFile(const QString& name)
{
    const QString dir = QCoreApplication::applicationDirPath() + "/wallpaper";
    QDir().mkpath(dir);
    return dir + "/" + name;
}

// 壁纸按 ui_settings 的 wallpaperAlpha（10-100，默认100）与主题底色混合后交给窗口
static QPixmap ComposeWallpaper(const QPixmap& src, ElaThemeType::ThemeMode mode)
{
    if (src.isNull()) {
        return src;
    }
    QSettings uiSettings(QCoreApplication::applicationDirPath() + "/ui_settings.ini",
                         QSettings::IniFormat);
    const int alpha = uiSettings.value("wallpaperAlpha", 100).toInt();
    if (alpha >= 100) {
        return src;
    }
    QPixmap canvas(src.size());
    canvas.fill(eTheme->getThemeColor(mode, ElaThemeType::WindowCentralStackBase));
    QPainter painter(&canvas);
    painter.setOpacity(qBound(10, alpha, 100) / 100.0);
    painter.drawPixmap(0, 0, src);
    painter.end();
    return canvas;
}

F_Setting::F_Setting(QWidget *parent):ElaScrollPage(parent)
{
    ElaWindow* window = dynamic_cast<ElaWindow*>(parent);

    // 壁纸：优先沿用上次会话缓存的图，没有才拉随机图（点"刷新图片/自定义壁纸"才会换）
    QPixmap lightImg(WallpaperFile("wallpaper_light.jpg"));
    QPixmap darkImg(WallpaperFile("wallpaper_dark.jpg"));
    if (lightImg.isNull() || darkImg.isNull()) {
        // 获取失败就留空，绝不用占位图覆盖缓存
        QPixmap fetchedLight = ranImg::instance()->getImgOrNull("pc");
        QThread::msleep(50);
        QPixmap fetchedDark = ranImg::instance()->getImgOrNull("pc");
        if (!fetchedLight.isNull()) {
            lightImg = fetchedLight;
            lightImg.save(WallpaperFile("wallpaper_light.jpg"), nullptr, 95);
        }
        if (!fetchedDark.isNull()) {
            darkImg = fetchedDark;
            darkImg.save(WallpaperFile("wallpaper_dark.jpg"), nullptr, 95);
        }
    }

    if (!lightImg.isNull()) {
        window->setWindowPixmap(ElaThemeType::Light, ComposeWallpaper(lightImg, ElaThemeType::Light));
    }
    if (!darkImg.isNull()) {
        window->setWindowPixmap(ElaThemeType::Dark, ComposeWallpaper(darkImg, ElaThemeType::Dark));
    }
    setWindowTitle("设置");

    eTheme->setThemeColor(ElaThemeType::Dark, ElaThemeType::BasicText, QColor(255, 255, 255));
    _themeComboBox = new ElaComboBox(this);
    _themeComboBox->addItem("日间模式");
    _themeComboBox->addItem("夜间模式");

    ElaScrollPageArea* themeSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* themeSwitchLayout = new QHBoxLayout(themeSwitchArea);
    ElaText* themeSwitchText = new ElaText("主题切换", this);
    themeSwitchText->setWordWrap(false);
    themeSwitchText->setTextPixelSize(15);
    themeSwitchLayout->addWidget(themeSwitchText);
    ElaText* themeText = new ElaText("主题设置", this);
    themeText->setWordWrap(false);
    themeText->setTextPixelSize(18);
    // 强制重新设置颜色
    themeSwitchLayout->addStretch();
    themeSwitchLayout->addWidget(_themeComboBox);
    connect(_themeComboBox, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [=](int index) {
        if (index == 0)
        {
            eTheme->setThemeMode(ElaThemeType::Light);
        }
        else
        {
            eTheme->setThemeMode(ElaThemeType::Dark);
        }
        saveUiSettings("theme", index);
    });
    connect(eTheme, &ElaTheme::themeModeChanged, _themeComboBox, [=]() {
        if (eTheme->getThemeMode() == ElaThemeType::Dark) {
            _themeComboBox->setStyleSheet("QComboBox { color: white; } QComboBox QAbstractItemView { color: white; background-color: #1E1E1E; }");
        } else {
            _themeComboBox->setStyleSheet("QComboBox { color: black; } QComboBox QAbstractItemView { color: black; }");
        }
        _themeComboBox->update();


        if (eTheme->getThemeMode() == ElaThemeType::Dark) {
            _otherComboBox->setStyleSheet("QComboBox { color: white; } QComboBox QAbstractItemView { color: white; background-color: #1E1E1E; }");
        } else {
            _otherComboBox->setStyleSheet("QComboBox { color: black; } QComboBox QAbstractItemView { color: black; }");
        }
        _otherComboBox->update();

    });

    ElaText* windowPaintText = new ElaText("主窗口绘制设置", this);
    windowPaintText->setWordWrap(false);
    windowPaintText->setTextPixelSize(15);

    _windowNormalButton = new ElaRadioButton("默认", this);
    _windowNormalButton->setChecked(true);
    _windowPixmapButton = new ElaRadioButton("随机二次元主题图", this);
    ElaPushButton* refreshButton = new ElaPushButton("刷新随机图片", this);
    refreshButton->setFixedHeight(38);
    refreshButton->setText("刷新图片");
    refreshButton->setFixedWidth(112);

    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        // 同步下拉框状态
        _themeComboBox->blockSignals(true);
        if (themeMode == ElaThemeType::Light)
        {
            _themeComboBox->setCurrentIndex(0);
        }
        else
        {
            _themeComboBox->setCurrentIndex(1);
        }
        _themeComboBox->blockSignals(false);

        QTimer::singleShot(700, this, [=]() {
            // 切主题重设壁纸时读缓存，不再换新图
            if (themeMode == ElaThemeType::Light)
            {
                QPixmap darkImg(WallpaperFile("wallpaper_dark.jpg"));
                if (!darkImg.isNull()) {
                    window->setWindowPixmap(ElaThemeType::Dark,
                                            ComposeWallpaper(darkImg, ElaThemeType::Dark));
                }
            }
            else
            {
                QPixmap lightImg(WallpaperFile("wallpaper_light.jpg"));
                if (!lightImg.isNull()) {
                    window->setWindowPixmap(ElaThemeType::Light,
                                            ComposeWallpaper(lightImg, ElaThemeType::Light));
                }
            }
            window->update();
        });
    });

    connect(refreshButton, &ElaToolButton::clicked, this, [=]() {
        QPixmap lightImg = ranImg::instance()->getImgOrNull("pc");
        QThread::msleep(50);
        QPixmap darkImg = ranImg::instance()->getImgOrNull("pc");
        if (lightImg.isNull() && darkImg.isNull()) {
            ADDMSG(ElaMessageBarType::Top, "网络图片获取失败，已保留原壁纸", this, 0, 3000);
            return;
        }
        // 落盘缓存：重启沿用本次刷新的图（高质量 JPG，控制体积）；失败的那套不覆盖旧缓存
        if (!lightImg.isNull()) {
            lightImg.save(WallpaperFile("wallpaper_light.jpg"), nullptr, 95);
            window->setWindowPixmap(ElaThemeType::Light, ComposeWallpaper(lightImg, ElaThemeType::Light));
        }
        if (!darkImg.isNull()) {
            darkImg.save(WallpaperFile("wallpaper_dark.jpg"), nullptr, 95);
            window->setWindowPixmap(ElaThemeType::Dark, ComposeWallpaper(darkImg, ElaThemeType::Dark));
        }
        window->update();

    });

    // 自定义壁纸：本地图片作为壁纸并缓存，重启沿用
    ElaPushButton* customWallpaperBtn = new ElaPushButton("自定义壁纸", this);
    customWallpaperBtn->setFixedHeight(38);
    customWallpaperBtn->setMinimumWidth(124);
    connect(customWallpaperBtn, &ElaPushButton::clicked, this, [=]() {
        QString file = QFileDialog::getOpenFileName(this, "选择壁纸图片", QString(),
                                                    "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)");
        if (file.isEmpty()) {
            return;
        }
        QPixmap custom(file);
        if (custom.isNull()) {
            return;
        }
        // 原样文件复制到缓存（无损，QPixmap 按内容识别格式，不受缓存文件扩展名影响）
        for (const QString& name : {QString("wallpaper_light.jpg"), QString("wallpaper_dark.jpg")}) {
            const QString dest = WallpaperFile(name);
            QFile::remove(dest);
            QFile::copy(file, dest);
        }
        window->setWindowPixmap(ElaThemeType::Light, ComposeWallpaper(custom, ElaThemeType::Light));
        window->setWindowPixmap(ElaThemeType::Dark, ComposeWallpaper(custom, ElaThemeType::Dark));
        // 壁纸要可见：切到图片绘制模式（复用既有信号链持久化 paintMode）
        _windowPixmapButton->setChecked(true);
        window->update();
    });

    QButtonGroup* windowPaintButtonGroup = new QButtonGroup(this);
    windowPaintButtonGroup->addButton(_windowNormalButton, 0);
    windowPaintButtonGroup->addButton(_windowPixmapButton, 1);
    windowPaintButtonGroup->addButton(refreshButton, 2);

    connect(windowPaintButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            window->setWindowPaintMode((ElaWindowType::PaintMode)windowPaintButtonGroup->id(button));
            saveUiSettings("paintMode", windowPaintButtonGroup->id(button));
        }

    });
    connect(window, &ElaWindow::pWindowPaintModeChanged, this, [=]() {
        auto button = windowPaintButtonGroup->button(window->getWindowPaintMode());
        ElaRadioButton* elaRadioButton = dynamic_cast<ElaRadioButton*>(button);
        if (elaRadioButton)
        {
            elaRadioButton->setChecked(true);
        }

    });

    ElaScrollPageArea* windowPaintModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* windowPaintModeLayout = new QHBoxLayout(windowPaintModeArea);
    windowPaintModeLayout->setSpacing(10);
    windowPaintModeLayout->addWidget(windowPaintText);
    windowPaintModeLayout->addStretch();
    windowPaintModeLayout->addWidget(_windowNormalButton);
    windowPaintModeLayout->addWidget(_windowPixmapButton);
    windowPaintModeLayout->addWidget(refreshButton);
    windowPaintModeLayout->addWidget(customWallpaperBtn);

    // 壁纸透明度：与主题底色混合的 alpha（10-100，默认 100=原图不透明）
    ElaSlider* wallpaperAlphaSlider = new ElaSlider(Qt::Horizontal, this);
    wallpaperAlphaSlider->setRange(10, 100);
    wallpaperAlphaSlider->setFixedWidth(220);
    ElaText* wallpaperAlphaText = new ElaText("壁纸透明度", this);
    wallpaperAlphaText->setWordWrap(false);
    wallpaperAlphaText->setTextPixelSize(15);
    ElaText* wallpaperAlphaValue = new ElaText("100%", this);
    wallpaperAlphaValue->setWordWrap(false);
    wallpaperAlphaValue->setTextPixelSize(15);
    ElaScrollPageArea* wallpaperAlphaArea = new ElaScrollPageArea(this);
    QHBoxLayout* wallpaperAlphaLayout = new QHBoxLayout(wallpaperAlphaArea);
    wallpaperAlphaLayout->addWidget(wallpaperAlphaText);
    wallpaperAlphaLayout->addStretch();
    wallpaperAlphaLayout->addWidget(wallpaperAlphaSlider);
    wallpaperAlphaLayout->addWidget(wallpaperAlphaValue);
    {
        QSettings alphaSettings(QCoreApplication::applicationDirPath() + "/ui_settings.ini",
                                QSettings::IniFormat);
        wallpaperAlphaSlider->setValue(alphaSettings.value("wallpaperAlpha", 100).toInt());
    }
    wallpaperAlphaValue->setText(QStringLiteral("%1%").arg(wallpaperAlphaSlider->value()));
    connect(wallpaperAlphaSlider, &QSlider::valueChanged, this, [=](int value) {
        saveUiSettings("wallpaperAlpha", value);
        wallpaperAlphaValue->setText(QStringLiteral("%1%").arg(value));
        // 两套主题的壁纸都按新透明度重新混合（拖动即时预览）
        QPixmap lightCache(WallpaperFile("wallpaper_light.jpg"));
        if (!lightCache.isNull()) {
            window->setWindowPixmap(ElaThemeType::Light,
                                    ComposeWallpaper(lightCache, ElaThemeType::Light));
        }
        QPixmap darkCache(WallpaperFile("wallpaper_dark.jpg"));
        if (!darkCache.isNull()) {
            window->setWindowPixmap(ElaThemeType::Dark,
                                    ComposeWallpaper(darkCache, ElaThemeType::Dark));
        }
        window->update();
    });

    // 导航栏模式选择
    ElaText* navigationText = new ElaText("导航栏设置", this);
    navigationText->setWordWrap(false);
    navigationText->setTextPixelSize(18);

    _minimumButton = new ElaRadioButton("隐藏", this);
    _compactButton = new ElaRadioButton("紧凑", this);
    _maximumButton = new ElaRadioButton("最大化", this);
    _autoButton = new ElaRadioButton("自动", this);
    _autoButton->setChecked(true);

    ElaScrollPageArea* displayModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* displayModeLayout = new QHBoxLayout(displayModeArea);
    ElaText* displayModeText = new ElaText("导航栏模式选择", this);
    displayModeText->setWordWrap(false);
    displayModeText->setTextPixelSize(15);
    displayModeLayout->addWidget(displayModeText);
    displayModeLayout->addStretch();
    displayModeLayout->addWidget(_minimumButton);
    displayModeLayout->addWidget(_compactButton);
    displayModeLayout->addWidget(_maximumButton);
    displayModeLayout->addWidget(_autoButton);

    QButtonGroup* navigationGroup = new QButtonGroup(this);
    navigationGroup->addButton(_autoButton, 0);
    navigationGroup->addButton(_minimumButton, 1);
    navigationGroup->addButton(_compactButton, 2);
    navigationGroup->addButton(_maximumButton, 3);

    connect(navigationGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled) {
            window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navigationGroup->id(button));
            saveUiSettings("navMode", navigationGroup->id(button));
        }
    });

    _noneButton = new ElaRadioButton("无特效", this);
    _popupButton = new ElaRadioButton("弹出", this);
    _popupButton->setChecked(true);
    _scaleButton = new ElaRadioButton("缩放", this);
    _flipButton = new ElaRadioButton("翻转", this);
    _blurButton = new ElaRadioButton("模糊", this);

    ElaScrollPageArea* stackSwitchModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* stackSwitchModeLayout = new QHBoxLayout(stackSwitchModeArea);
    ElaText* stackSwitchModeText = new ElaText("界面切换效果选择", this);
    stackSwitchModeText->setWordWrap(false);
    stackSwitchModeText->setTextPixelSize(15);
    stackSwitchModeLayout->addWidget(stackSwitchModeText);
    stackSwitchModeLayout->addStretch();
    stackSwitchModeLayout->addWidget(_noneButton);
    stackSwitchModeLayout->addWidget(_popupButton);
    stackSwitchModeLayout->addWidget(_scaleButton);
    stackSwitchModeLayout->addWidget(_flipButton);
    stackSwitchModeLayout->addWidget(_blurButton);

    QButtonGroup* stackSwitchGroup = new QButtonGroup(this);
    stackSwitchGroup->addButton(_noneButton, 0);
    stackSwitchGroup->addButton(_popupButton, 1);
    stackSwitchGroup->addButton(_scaleButton, 2);
    stackSwitchGroup->addButton(_flipButton, 3);
    stackSwitchGroup->addButton(_blurButton, 4);

    connect(stackSwitchGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled) {
            window->setStackSwitchMode((ElaWindowType::StackSwitchMode)stackSwitchGroup->id(button));
            saveUiSettings("stackMode", stackSwitchGroup->id(button));
        }

    });

    ElaText* functionText = new ElaText("功能设置", this);
    functionText->setWordWrap(false);
    functionText->setTextPixelSize(18);


    // 用户卡片开关
    _userCardSwitchButton = new ElaToggleSwitch(this);
    ElaScrollPageArea* userCardSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* userCardSwitchLayout = new QHBoxLayout(userCardSwitchArea);
    ElaText* userCardSwitchText = new ElaText("隐藏用户卡片", this);
    userCardSwitchText->setWordWrap(false);
    userCardSwitchText->setTextPixelSize(15);
    userCardSwitchLayout->addWidget(userCardSwitchText);
    userCardSwitchLayout->addStretch();
    userCardSwitchLayout->addWidget(_userCardSwitchButton);
    // TODO: 实现用户卡片隐藏逻辑
    connect(_userCardSwitchButton, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        window->setUserInfoCardVisible(!checked);
        saveUiSettings("userCard", checked);
    });

    ElaText* micaSwitchText = new ElaText("窗口效果", this);
    micaSwitchText->setWordWrap(false);
    micaSwitchText->setTextPixelSize(15);
    _normalButton = new ElaRadioButton("普通", this);
    _elaMicaButton = new ElaRadioButton("新云母", this);
#ifdef Q_OS_WIN
    _micaButton = new ElaRadioButton("云母", this);
    _micaAltButton = new ElaRadioButton("云母-替代", this);
    _acrylicButton = new ElaRadioButton("亚克力", this);
    _dwmBlurnormalButton = new ElaRadioButton("DWM模糊", this);
#endif
    _normalButton->setChecked(true);
    QButtonGroup* displayButtonGroup = new QButtonGroup(this);
    displayButtonGroup->addButton(_normalButton, 0);
    displayButtonGroup->addButton(_elaMicaButton, 1);
#ifdef Q_OS_WIN
    displayButtonGroup->addButton(_micaButton, 2);
    displayButtonGroup->addButton(_micaAltButton, 3);
    displayButtonGroup->addButton(_acrylicButton, 4);
    displayButtonGroup->addButton(_dwmBlurnormalButton, 5);
#endif
    connect(displayButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled)
        {
            eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)displayButtonGroup->id(button));
            saveUiSettings("displayMode", displayButtonGroup->id(button));
        }
    });
    connect(eApp, &ElaApplication::pWindowDisplayModeChanged, this, [=]() {
        auto button = displayButtonGroup->button(eApp->getWindowDisplayMode());
        ElaRadioButton* elaRadioButton = dynamic_cast<ElaRadioButton*>(button);
        if (elaRadioButton)
        {
            elaRadioButton->setChecked(true);
        }
    });
    ElaScrollPageArea* micaSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* micaSwitchLayout = new QHBoxLayout(micaSwitchArea);
    micaSwitchLayout->addWidget(micaSwitchText);
    micaSwitchLayout->addStretch();
    micaSwitchLayout->addWidget(_normalButton);
    micaSwitchLayout->addWidget(_elaMicaButton);
#ifdef Q_OS_WIN
    micaSwitchLayout->addWidget(_micaButton);
    micaSwitchLayout->addWidget(_micaAltButton);
    micaSwitchLayout->addWidget(_acrylicButton);
    micaSwitchLayout->addWidget(_dwmBlurnormalButton);
#endif


    // 其他设置
    ElaText* otherText = new ElaText("其他设置", this);
    otherText->setWordWrap(false);
    otherText->setTextPixelSize(18);

    _otherComboBox = new ElaComboBox(this);
    _otherComboBox->addItem("拒绝任何人添加好友");
    _otherComboBox->addItem("发送验证信息添加");
    _otherComboBox->addItem("允许任何人添加好友");

    ElaScrollPageArea* otherArea = new ElaScrollPageArea(this);
    QHBoxLayout* otherLayout = new QHBoxLayout(otherArea);
    ElaText* otherComboBoxText = new ElaText("加好友设置", this);
    otherComboBoxText->setWordWrap(false);
    otherComboBoxText->setTextPixelSize(15);
    otherLayout->addWidget(otherComboBoxText);
    otherLayout->addStretch();
    otherLayout->addWidget(_otherComboBox);
    connect(_otherComboBox, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [=](int index) {
        saveUiSettings("addFriendMode", index);
        // 同步到服务端（Redis 策略，供他人加好友时校验）
        QJsonObject obj;
        obj["uid"] = UserMgr::GetInstance()->GetUid();
        obj["policy"] = index;
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_SET_ADD_POLICY_REQ,
                                                 QJsonDocument(obj).toJson(QJsonDocument::Compact));
    });

    // 通用设置
    ElaText* generalText = new ElaText("通用设置", this);
    generalText->setWordWrap(false);
    generalText->setTextPixelSize(18);

    ElaComboBox* fontSizeCombo = new ElaComboBox(this);
    fontSizeCombo->addItem("小");
    fontSizeCombo->addItem("标准");
    fontSizeCombo->addItem("大");
    ElaScrollPageArea* fontArea = new ElaScrollPageArea(this);
    QHBoxLayout* fontLayout = new QHBoxLayout(fontArea);
    ElaText* fontText = new ElaText("字体大小", this);
    fontText->setWordWrap(false);
    fontText->setTextPixelSize(15);
    fontLayout->addWidget(fontText);
    fontLayout->addStretch();
    fontLayout->addWidget(fontSizeCombo);
    // 进设置页时的字体 = 未被本功能改动的原始默认字体，"标准"档原样还原
    const QFont defaultFont = qApp->font();
    connect(fontSizeCombo, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [=](int index) {
        // 只调字号不换字体族：小 8pt / 标准 = 原始默认 / 大 12pt
        QFont font = defaultFont;
        if (index == 0) {
            font.setPointSize(8);
        }
        else if (index == 2) {
            font.setPointSize(12);
        }
        qApp->setFont(font);
        saveUiSettings("fontSize", index);
    });

    // 消息通知开关：控制右下角通知弹窗（新消息/好友申请），偏好持久化
    ElaToggleSwitch* notifySwitch = new ElaToggleSwitch(this);
    ElaScrollPageArea* notifyArea = new ElaScrollPageArea(this);
    QHBoxLayout* notifyLayout = new QHBoxLayout(notifyArea);
    ElaText* notifyText = new ElaText("消息通知", this);
    notifyText->setWordWrap(false);
    notifyText->setTextPixelSize(15);
    notifyLayout->addWidget(notifyText);
    notifyLayout->addStretch();
    notifyLayout->addWidget(notifySwitch);
    connect(notifySwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        saveUiSettings("notifyOn", checked);
    });

    ElaPushButton* checkUpdateBtn = new ElaPushButton("检测更新", this);
    checkUpdateBtn->setFixedHeight(38);
    checkUpdateBtn->setMinimumWidth(112);
    ElaScrollPageArea* updateArea = new ElaScrollPageArea(this);
    QHBoxLayout* updateLayout = new QHBoxLayout(updateArea);
    ElaText* updateText = new ElaText("版本更新", this);
    updateText->setWordWrap(false);
    updateText->setTextPixelSize(15);
    updateLayout->addWidget(updateText);
    updateLayout->addStretch();
    updateLayout->addWidget(checkUpdateBtn);
    connect(checkUpdateBtn, &ElaPushButton::clicked, this, [=]() {
        ADDMSG(ElaMessageBarType::Top, "当前已是最新版本", this, 1);
    });

    // AI 助手配置：写回 config.ini [ai] 段，保存后热生效（不用重启）
    ElaText* aiText = new ElaText("AI 助手", this);
    aiText->setWordWrap(false);
    aiText->setTextPixelSize(18);

    QSettings aiCfg(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);

    ElaLineEdit* aiUrlEdit = new ElaLineEdit(this);
    aiUrlEdit->setPlaceholderText("接口地址，如 https://api.openai.com/v1");
    aiUrlEdit->setText(aiCfg.value("ai/base_url").toString());
    aiUrlEdit->setMinimumWidth(300);
    ElaScrollPageArea* aiUrlArea = new ElaScrollPageArea(this);
    QHBoxLayout* aiUrlLayout = new QHBoxLayout(aiUrlArea);
    ElaText* aiUrlText = new ElaText("接口地址", this);
    aiUrlText->setWordWrap(false);
    aiUrlText->setTextPixelSize(15);
    aiUrlLayout->addWidget(aiUrlText);
    aiUrlLayout->addStretch();
    aiUrlLayout->addWidget(aiUrlEdit);

    ElaLineEdit* aiModelEdit = new ElaLineEdit(this);
    aiModelEdit->setPlaceholderText("模型名，如 deepseek-v4-flash");
    aiModelEdit->setText(aiCfg.value("ai/model").toString());
    aiModelEdit->setMinimumWidth(300);
    ElaScrollPageArea* aiModelArea = new ElaScrollPageArea(this);
    QHBoxLayout* aiModelLayout = new QHBoxLayout(aiModelArea);
    ElaText* aiModelText = new ElaText("模型", this);
    aiModelText->setWordWrap(false);
    aiModelText->setTextPixelSize(15);
    aiModelLayout->addWidget(aiModelText);
    aiModelLayout->addStretch();
    aiModelLayout->addWidget(aiModelEdit);

    ElaLineEdit* aiKeyEdit = new ElaLineEdit(this);
    aiKeyEdit->setEchoMode(QLineEdit::Password);
    aiKeyEdit->setPlaceholderText("API Key");
    aiKeyEdit->setText(aiCfg.value("ai/api_key").toString());
    aiKeyEdit->setMinimumWidth(300);
    ElaScrollPageArea* aiKeyArea = new ElaScrollPageArea(this);
    QHBoxLayout* aiKeyLayout = new QHBoxLayout(aiKeyArea);
    ElaText* aiKeyText = new ElaText("API 密钥", this);
    aiKeyText->setWordWrap(false);
    aiKeyText->setTextPixelSize(15);
    aiKeyLayout->addWidget(aiKeyText);
    aiKeyLayout->addStretch();
    aiKeyLayout->addWidget(aiKeyEdit);

    ElaPushButton* aiSaveBtn = new ElaPushButton("保存配置", this);
    aiSaveBtn->setFixedHeight(38);
    aiSaveBtn->setMinimumWidth(112);
    ElaScrollPageArea* aiSaveArea = new ElaScrollPageArea(this);
    QHBoxLayout* aiSaveLayout = new QHBoxLayout(aiSaveArea);
    ElaText* aiSaveText = new ElaText("保存并立即生效", this);
    aiSaveText->setWordWrap(false);
    aiSaveText->setTextPixelSize(15);
    aiSaveLayout->addWidget(aiSaveText);
    aiSaveLayout->addStretch();
    aiSaveLayout->addWidget(aiSaveBtn);
    connect(aiSaveBtn, &ElaPushButton::clicked, this, [=]() {
        const QString baseUrl = aiUrlEdit->text().trimmed();
        const QString model = aiModelEdit->text().trimmed();
        const QString apiKey = aiKeyEdit->text().trimmed();
        if (baseUrl.isEmpty() || model.isEmpty() || apiKey.isEmpty()) {
            ADDMSG(ElaMessageBarType::Top, "接口地址/模型/密钥不能为空", this, 0, 3000);
            return;
        }
        QSettings cfg(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
        cfg.setValue("ai/base_url", baseUrl);
        cfg.setValue("ai/model", model);
        cfg.setValue("ai/api_key", apiKey);
        cfg.sync();
        AiMgr::GetInstance()->Init(baseUrl, model, apiKey);
        ADDMSG(ElaMessageBarType::Top, "AI 配置已保存", this, 1, 2000);
    });

    // 存储管理：清理背景图片缓存
    ElaText* storageText = new ElaText("存储管理", this);
    storageText->setWordWrap(false);
    storageText->setTextPixelSize(18);

    ElaPushButton* clearCacheBtn = new ElaPushButton("清理缓存", this);
    clearCacheBtn->setFixedHeight(38);
    clearCacheBtn->setMinimumWidth(112);
    ElaPushButton* clearLogBtn = new ElaPushButton("清理日志", this);
    clearLogBtn->setFixedHeight(38);
    clearLogBtn->setMinimumWidth(112);
    ElaScrollPageArea* storageArea = new ElaScrollPageArea(this);
    QHBoxLayout* storageLayout = new QHBoxLayout(storageArea);
    ElaText* storageDesc = new ElaText("背景图片缓存 / 历史日志", this);
    storageDesc->setWordWrap(false);
    storageDesc->setTextPixelSize(15);
    storageLayout->addWidget(storageDesc);
    storageLayout->addStretch();
    storageLayout->addWidget(clearCacheBtn);
    storageLayout->addWidget(clearLogBtn);
    connect(clearCacheBtn, &ElaPushButton::clicked, this, [=]() {
        const QString dir = QCoreApplication::applicationDirPath() + "/wallpaper";
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);
        // 图片绘制模式下补一张新随机图，避免壁纸空缺
        if (_windowPixmapButton->isChecked()) {
            QPixmap lightImg = ranImg::instance()->getImgOrNull("pc");
            QThread::msleep(50);
            QPixmap darkImg = ranImg::instance()->getImgOrNull("pc");
            if (!lightImg.isNull()) {
                lightImg.save(WallpaperFile("wallpaper_light.jpg"), nullptr, 95);
                window->setWindowPixmap(ElaThemeType::Light, ComposeWallpaper(lightImg, ElaThemeType::Light));
            }
            if (!darkImg.isNull()) {
                darkImg.save(WallpaperFile("wallpaper_dark.jpg"), nullptr, 95);
                window->setWindowPixmap(ElaThemeType::Dark, ComposeWallpaper(darkImg, ElaThemeType::Dark));
            }
            window->update();
        }
        ADDMSG(ElaMessageBarType::Top, "背景图片缓存已清理", this, 1);
    });
    connect(clearLogBtn, &ElaPushButton::clicked, this, [=]() {
        // 与 Logger 的 log/ 相对路径一致
        QDir logDir(QStringLiteral("log"));
        const QString today = QDate::currentDate().toString("yyyyMMdd");
        int removed = 0;
        const auto entries = logDir.entryList(QStringList() << "foolchat_*.log",
                                              QDir::Files, QDir::Name);
        for (const QString& name : entries) {
            if (name == "foolchat_" + today + ".log") {
                continue;
            }
            if (logDir.remove(name)) {
                ++removed;
            }
        }
        // 当天文件被 Logger 占用删不掉，调 clear() 清空内容（内部会关→截断→重开）
        Logger::getInstance().clear();
        ADDMSG(ElaMessageBarType::Top,
               QString("已清理 %1 个历史日志，当天日志已清空").arg(removed),
               this, 1);
    });

    ElaPushButton* clearMusicBtn = new ElaPushButton("清理音乐库", this);
    clearMusicBtn->setFixedHeight(38);
    clearMusicBtn->setMinimumWidth(124);
    ElaScrollPageArea* musicArea = new ElaScrollPageArea(this);
    QHBoxLayout* musicLayout = new QHBoxLayout(musicArea);
    ElaText* musicDesc = new ElaText("音乐盒歌单（不动音频原文件）", this);
    musicDesc->setWordWrap(false);
    musicDesc->setTextPixelSize(15);
    musicLayout->addWidget(musicDesc);
    musicLayout->addStretch();
    musicLayout->addWidget(clearMusicBtn);
    connect(clearMusicBtn, &ElaPushButton::clicked, this, [=]() {
        // 只清歌单记录，不动磁盘上的音频原文件
        if (QSqlDatabase::contains("music_sqlite")) {
            QSqlDatabase musicDb = QSqlDatabase::database("music_sqlite");
            if (musicDb.isOpen()) {
                QSqlQuery musicQuery(musicDb);
                musicQuery.exec("DELETE FROM songs");
            }
        }
        else {
            QFile::remove("music_player.db");
        }
        ADDMSG(ElaMessageBarType::Top, "音乐库已清理（音频原文件未动）", this, 1);
    });

    // 数据管理：清理登录历史
    ElaText* dataText = new ElaText("数据管理", this);
    dataText->setWordWrap(false);
    dataText->setTextPixelSize(18);

    ElaPushButton* clearHistoryBtn = new ElaPushButton("清理登录历史", this);
    clearHistoryBtn->setFixedHeight(38);
    clearHistoryBtn->setMinimumWidth(140);
    ElaScrollPageArea* dataArea = new ElaScrollPageArea(this);
    QHBoxLayout* dataLayout = new QHBoxLayout(dataArea);
    ElaText* dataDesc = new ElaText("登录历史（账号与记住的密码）", this);
    dataDesc->setWordWrap(false);
    dataDesc->setTextPixelSize(15);
    dataLayout->addWidget(dataDesc);
    dataLayout->addStretch();
    dataLayout->addWidget(clearHistoryBtn);
    connect(clearHistoryBtn, &ElaPushButton::clicked, this, [=]() {
        QSettings settings(QCoreApplication::applicationDirPath() + "/accounts.ini", QSettings::IniFormat);
        settings.remove("accounts");
        settings.remove("last_user");
        ADDMSG(ElaMessageBarType::Top, "登录历史已清理", this, 1);
    });

    // 个人资料
    ElaText* profileT = new ElaText("个人信息", this);
    profileT->setWordWrap(false);
    profileT->setTextPixelSize(18);

    ElaScrollPageArea* profileArea = new ElaScrollPageArea(this);
    QHBoxLayout* profileLayout = new QHBoxLayout(profileArea);
    ElaText* profileText = new ElaText("个人资料", this);
    profileText->setWordWrap(false);
    profileText->setTextPixelSize(15);
    ElaPushButton* editProfileBtn = new ElaPushButton("编辑资料", this);
    editProfileBtn->setFixedHeight(38);
    editProfileBtn->setMinimumWidth(112);
    connect(editProfileBtn, &ElaPushButton::clicked, this, [this]() {
        auto* dlg = new EditProfileDlg(this->window());
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    profileLayout->addWidget(profileText);
    profileLayout->addStretch();
    profileLayout->addWidget(editProfileBtn);

    // 创建中心容器
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("Setting");

    // 垂直布局
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);


    centerLayout->addSpacing(10);
    centerLayout->addWidget(profileT);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(profileArea);
    centerLayout->addSpacing(30);
    centerLayout->addWidget(themeText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(themeSwitchArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(micaSwitchArea);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(windowPaintText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(windowPaintModeArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(wallpaperAlphaArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(navigationText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(displayModeArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(stackSwitchModeArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(functionText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(userCardSwitchArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(otherText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(otherArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(generalText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(fontArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(notifyArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(aiText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(aiUrlArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(aiModelArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(aiKeyArea);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(aiSaveArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(updateArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(storageText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(storageArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(musicArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(dataText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(dataArea);
    centerLayout->addStretch();

    // 恢复上次会话的界面偏好（setChecked/setCurrentIndex 触发既有信号自动生效）
    QSettings uiSettings(QCoreApplication::applicationDirPath() + "/ui_settings.ini", QSettings::IniFormat);
    _themeComboBox->setCurrentIndex(uiSettings.value("theme", 0).toInt());
    // 加好友策略以服务端为准（登录时带回）
    _otherComboBox->setCurrentIndex(UserMgr::GetInstance()->GetAddPolicy());
    // 只有用户主动设置过字体才应用，否则保持 Qt 默认字体
    if (uiSettings.contains("fontSize")) {
        fontSizeCombo->setCurrentIndex(uiSettings.value("fontSize").toInt());
    }
    // 消息通知默认开：仅当用户显式关过才恢复为关
    notifySwitch->setIsToggled(uiSettings.value("notifyOn", true).toBool());
    if (uiSettings.value("userCard", false).toBool()) {
        _userCardSwitchButton->setIsToggled(true);
        // setter 不保证发信号，显式应用一次
        window->setUserInfoCardVisible(false);
    }
    if (uiSettings.value("paintMode", 0).toInt() == 1) {
        _windowPixmapButton->setChecked(true);
    }
    int navMode = uiSettings.value("navMode", 3).toInt();
    if (navMode == 0) {
        _minimumButton->setChecked(true);
    }
    else if (navMode == 1) {
        _compactButton->setChecked(true);
    }
    else if (navMode == 2) {
        _maximumButton->setChecked(true);
    }
    int stackMode = uiSettings.value("stackMode", 1).toInt();
    if (stackMode == 0) {
        _noneButton->setChecked(true);
    }
    else if (stackMode == 2) {
        _scaleButton->setChecked(true);
    }
    else if (stackMode == 3) {
        _flipButton->setChecked(true);
    }
    else if (stackMode == 4) {
        _blurButton->setChecked(true);
    }
    int displayMode = uiSettings.value("displayMode", 0).toInt();
    if (displayMode == 1) {
        _elaMicaButton->setChecked(true);
    }
#ifdef Q_OS_WIN
    else if (displayMode == 2) {
        _micaButton->setChecked(true);
    }
    else if (displayMode == 3) {
        _micaAltButton->setChecked(true);
    }
    else if (displayMode == 4) {
        _acrylicButton->setChecked(true);
    }
    else if (displayMode == 5) {
        _dwmBlurnormalButton->setChecked(true);
    }
#endif

    addCentralWidget(centralWidget, true, true, 0);

}

void F_Setting::saveUiSettings(const QString& key, const QVariant& value)
{
    // 不能存 config.ini：构建产物会被 PostBuild 覆盖
    QSettings settings(QCoreApplication::applicationDirPath() + "/ui_settings.ini", QSettings::IniFormat);
    settings.setValue(key, value);
}

F_Setting::~F_Setting()
{

}


