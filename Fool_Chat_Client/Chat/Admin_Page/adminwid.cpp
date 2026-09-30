#include "adminwid.h"
#include "ui_adminwid.h"
#include <QWebEngineView>

namespace {
// 后台管理服务地址
const char *kAdminUrl = "http://127.0.0.1:9100/";
}

AdminWid::AdminWid(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AdminWid)
{
    ui->setupUi(this);

    QWebEngineView *view = new QWebEngineView(ui->webHost);
    view->load(QUrl(kAdminUrl));
    ui->webHostLayout->addWidget(view);
}

AdminWid::~AdminWid()
{
    delete ui;
}
