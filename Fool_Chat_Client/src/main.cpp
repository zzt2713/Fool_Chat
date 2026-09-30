#include "ui/mainwindow.h"
#include "ElaApplication.h"
#include "ElaTheme.h"
#include "core/global.h"
#include <QSqlDatabase>
#include <QLoggingCategory>
#include <QPalette>
#include "core/tcpmgr.h"
#include "core/aimgr.h"

namespace {
QPalette makeAppPalette(ElaThemeType::ThemeMode mode)
{
    if (mode == ElaThemeType::Light) {
        return QPalette();
    }
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(53, 53, 53));
    pal.setColor(QPalette::WindowText, Qt::white);
    pal.setColor(QPalette::Base, QColor(42, 42, 42));
    pal.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    pal.setColor(QPalette::ToolTipBase, Qt::white);
    pal.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
    pal.setColor(QPalette::Text, Qt::white);
    pal.setColor(QPalette::Button, QColor(53, 53, 53));
    pal.setColor(QPalette::ButtonText, Qt::white);
    pal.setColor(QPalette::BrightText, Qt::red);
    pal.setColor(QPalette::Link, QColor(42, 130, 218));
    pal.setColor(QPalette::LinkVisited, QColor(42, 130, 218));
    pal.setColor(QPalette::Highlight, QColor(42, 130, 218));
    pal.setColor(QPalette::HighlightedText, Qt::black);
    pal.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(128, 128, 128));
    return pal;
}
} // namespace

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    // 日志崩溃捕获：Qt 消息/未捕获异常/致命信号统一写入 log/
    Logger::installCrashHook();
    // Qt 自身的 info 噪音（多媒体后端、字体 OpenType 等）不落日志，只保留警告及以上；
    // Logger 自己的 LOG_* 直接写文件，不经过此过滤
    QLoggingCategory::setFilterRules(QStringLiteral("*.info=false"));

    // ELA初始化
    eApp->init();

    // 加载QSS样式
    QFile qss(":/style/stylesheet.qss");
    if(qss.open(QFile::ReadOnly)){
        QString style = QLatin1StringView(qss.readAll());
        a.setStyleSheet(style);
        qss.close();
    }else{
        LOG_ERROR("Err is main qss.open(QFile::ReadOnly).");
    }

    // 应用 palette 跟随 Ela 主题（创建于主题切换之后的标签/输入框靠它取色）
    a.setPalette(makeAppPalette(eTheme->getThemeMode()));
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, &a, [](ElaThemeType::ThemeMode mode) {
        qApp->setPalette(makeAppPalette(mode));
    });

    // 加载配置文件
    QString fileName = "config.ini";
    QString app_path = QCoreApplication::applicationDirPath();
    QString config_path = QDir::toNativeSeparators(app_path + QDir::separator() + fileName);

    QSettings settings(config_path,QSettings::IniFormat);
    QString gate_host = settings.value("GateServer/host").toString();
    QString gate_port = settings.value("GateServer/port").toString();
    gate_url_prefix = "http://" + gate_host + ":" + gate_port;

    // AI 机器人配置（[ai] 段，密钥不打印）
    AiMgr::GetInstance()->Init(settings.value("ai/base_url").toString(),
                               settings.value("ai/model").toString(),
                               settings.value("ai/api_key").toString());

    MainWindow w;
    // emit TcpMgr::GetInstance()->sig_switch_chatdlg();
    w.show();
    return a.exec();
}
