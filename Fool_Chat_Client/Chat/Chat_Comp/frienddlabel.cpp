#include "frienddlabel.h"
#include "ui_frienddlabel.h"
#include "ElaToolButton.h"
#include <QFontMetrics>

FrienddLabel::FrienddLabel(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::FrienddLabel)
{
    ui->setupUi(this);
    ui->close_tool_btn->setElaIcon(ElaIconType::CircleXmark);
    ui->close_tool_btn->setIsTransparent(true);
    connect(ui->close_tool_btn,&ElaToolButton::clicked,this,&FrienddLabel::slot_close);
}

FrienddLabel::~FrienddLabel()
{
    delete ui;
}

void FrienddLabel::SetText(QString text)
{
    _text = text;
    ui->tip_lb->setText(_text);

    // 先让标签自适应大小
    ui->tip_lb->adjustSize();

    // 获取字体信息
    QFontMetrics fontMetrics(ui->tip_lb->font());
    int textWidth = fontMetrics.horizontalAdvance(_text);
    int textHeight = fontMetrics.height();
    int contentHeight = qMax(ui->tip_lb->height(), ui->close_tool_btn->height());
    int padding = 8;  // 上下内边距
    int finalHeight = contentHeight + padding;

    // 宽度 = 标签宽度 + 按钮宽度 + 间距
    int spacing = 8;  // 标签和按钮之间的间距
    int finalWidth = ui->tip_lb->width() + ui->close_tool_btn->width() + spacing;

    this->setFixedSize(finalWidth, finalHeight);

    // 垂直居中放置子控件
    int centerY_tip = (finalHeight - ui->tip_lb->height()) / 2;
    int centerY_btn = (finalHeight - ui->close_tool_btn->height()) / 2;

    ui->tip_lb->move(4, centerY_tip);
    ui->close_tool_btn->move(ui->tip_lb->width() + spacing/2, centerY_btn);

    _width = this->width();
    _height = this->height();
}

int FrienddLabel::Width()
{
    return _width;
}

int FrienddLabel::Height()
{
    return _height;
}

QString FrienddLabel::Text()
{
    return _text;
}

void FrienddLabel::slot_close()
{
    emit sig_close(_text);
}
