#include "adminwid.h"
#include "ui_adminwid.h"
#include <QWebEngineView>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

namespace {
// 后台管理服务地址，实际地址由 config.ini 的 [admin] url 指定
const char *kAdminUrl = "http://127.0.0.1:9100/";
}

AdminWid::AdminWid(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AdminWid)
{
    ui->setupUi(this);

    const QString configPath = QDir::toNativeSeparators(
        QCoreApplication::applicationDirPath() + QDir::separator() + "config.ini");
    QSettings settings(configPath, QSettings::IniFormat);
    const QUrl adminUrl(settings.value("admin/url", kAdminUrl).toString());

    QWebEngineView *view = new QWebEngineView(ui->webHost);
    view->load(adminUrl);
    ui->webHostLayout->addWidget(view);
}

AdminWid::~AdminWid()
{
    delete ui;
}
