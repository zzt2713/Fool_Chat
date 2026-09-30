#include "findsuccessdlg.h"
#include "ui_findsuccessdlg.h"
#include <QDir>
#include <QColor>
#include "applyfriend.h"
#include "pixmaputil.h"

FindSuccessDlg::FindSuccessDlg(QWidget *parent)
    : ElaDialog(parent), ui(new Ui::FindSuccessDlg),_parent(parent)
{
    ui->setupUi(this);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setWindowTitle("添加联系人");
    // setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    QString app_path = QCoreApplication::applicationDirPath();
    QString pix_path = QDir::toNativeSeparators(app_path +
                                                QDir::separator() + "static"+ QDir::separator() + "head_1.jpg");
    QPixmap head_pix(pix_path);
    // 高保真圆图，避免按逻辑尺寸缩放在高 DPI 下发糊
    ui->head_lb->setPixmap(PixmapUtil::round(head_pix, ui->head_lb->size(), devicePixelRatioF()));
    ui->head_lb->setScaledContents(false);
    // 浅色模式
    ui->add_friend_btn->setLightDefaultColor(QColor(0x0078d4));
    ui->add_friend_btn->setLightHoverColor(QColor(0x106ebe));
    ui->add_friend_btn->setLightPressColor(QColor(0x005a9e));

    // 深色模式
    ui->add_friend_btn->setDarkDefaultColor(QColor(0x4a5c6b));
    ui->add_friend_btn->setDarkHoverColor(QColor(0x5a6c7b));
    ui->add_friend_btn->setDarkPressColor(QColor(0x3a4c5b));

    // 文字颜色 - 保持白色
    ui->add_friend_btn->setLightTextColor(QColor(0xFFFFFF));
    ui->add_friend_btn->setDarkTextColor(QColor(0xFFFFFF));

    this->setModal(true);

}

FindSuccessDlg::~FindSuccessDlg()
{
    delete ui;
}

void FindSuccessDlg::SetSearchInfo(std::shared_ptr<SearchInfo> si)
{
    _si = si;
    ui->name_lb->setText(_si->_name);
}

void FindSuccessDlg::on_add_friend_btn_clicked()
{
    this->hide();
    auto *apply = new ApplyFriend(_parent);
    apply->SetSearchInfo(_si);
    apply->setModal(true);
    apply->show();

}

