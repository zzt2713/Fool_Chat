#include "c_about.h"
#include "ui_c_about.h"
#include "ElaPersonPicture.h"

C_About::C_About(QWidget *parent)
    : ElaDialog(parent)
    , ui(new Ui::C_About)
{
    ui->setupUi(this);
    setFixedSize(400, 600);
    setWindowTitle("关于软件");
    setWindowIcon(QIcon(":/icons/image.png"));
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    ui->icon_wid->setPicture(QPixmap(":/res/head_4.jpg"));
    ui->icon_wid->setPictureSize(100);
    ui->content_lb->setText(""
        "版本更新日志 v2.5.0(内容测试)\n"
        "\n"
        "新增功能：\n"
        "1. 优化用户界面交互\n"
        "2. 修复已知bug\n"
        "3. 提升系统稳定性\n"
        "4. 添加代码编辑器\n"
    );

}

C_About::~C_About()
{
    delete ui;
}
