#include "newstar.h"
#include "ui_newstar.h"
#include "ElaImageCard.h"
#include "ElaIcon.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPalette>
#include <QColor>
#include <QShortcut>

NewStar::NewStar(QWidget *parent)
    : ElaDialog(parent)
    , ui(new Ui::NewStar)
{
    ui->setupUi(this);
    setWindowTitle("发布动态");
    this->setFixedSize(520, 480);

    ui->add_pic->setElaIcon(ElaIconType::Image);
    ui->add_pic->setToolTip("添加图片");

    ui->pic_wid->setViewMode(QListView::IconMode);
    ui->pic_wid->setIconSize(QSize(100, 100));
    ui->pic_wid->setResizeMode(QListView::Adjust);
    ui->pic_wid->setSpacing(8);
    ui->pic_wid->setMovement(QListView::Static);
    ui->pic_wid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->pic_wid->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);


    ShowPicWid(false);
    ui->wordCountLabel->setText("0/500");

    QPalette pal = ui->plainTextEdit->palette();
    pal.setColor(QPalette::PlaceholderText, QColor("#9a9a9a"));
    ui->plainTextEdit->setPalette(pal);

    for (int keyId : {int(Qt::Key_Return), int(Qt::Key_Enter)}) {
        auto* sc = new QShortcut(QKeySequence(Qt::CTRL | keyId), this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, &NewStar::on_pushButton_clicked);
    }
}

NewStar::~NewStar()
{
    delete ui;
}

void NewStar::on_pushButton_clicked()
{
    if (ui->plainTextEdit->toPlainText().trimmed().isEmpty() && _imageList.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入内容或添加图片");
        return;
    }

    emit sigPublish(ui->plainTextEdit->toPlainText().trimmed(), _imageList);
    accept();
}

void NewStar::on_add_pic_clicked()
{
    // 选择图片
    QStringList files = QFileDialog::getOpenFileNames(
        this, "选择图片", QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");
    if (files.isEmpty()) return;

    for (const QString &filePath : files) {
        QPixmap pix(filePath);
        if (!pix.isNull()) {
            _imageList.append(pix);
        }
    }

    // 更新图片列表
    updatePicList();
}

void NewStar::on_plainTextEdit_textChanged()
{
    int len = ui->plainTextEdit->toPlainText().length();
    ui->wordCountLabel->setText(QString("%1/500").arg(len));
}

void NewStar::ShowPicWid(bool b_ok)
{
    ui->pic_wid->setVisible(b_ok);
}

void NewStar::updatePicList()
{
    ui->pic_wid->clear();
    ShowPicWid(!_imageList.isEmpty());

    for (int i = 0; i < _imageList.size(); ++i) {
        QWidget *itemWidget = new QWidget();
        QVBoxLayout *vLayout = new QVBoxLayout(itemWidget);
        vLayout->setContentsMargins(0, 0, 0, 0);
        vLayout->setSpacing(4);

        ElaImageCard *card = new ElaImageCard();
        card->setCardImage(_imageList[i].toImage());
        card->setFixedSize(100, 100);
        card->setBorderRadius(8);
        vLayout->addWidget(card);

        ElaToolButton *deleteBtn = new ElaToolButton();
        deleteBtn->setElaIcon(ElaIconType::TrashCan);
        deleteBtn->setFixedSize(20, 20);
        deleteBtn->setToolTip("删除");
        deleteBtn->setStyleSheet("QToolButton { border: none; background: transparent; }");
        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        btnLayout->addWidget(deleteBtn);
        btnLayout->addStretch();
        vLayout->addLayout(btnLayout);

        connect(deleteBtn, &QToolButton::clicked, this, [this, i]() {
            _imageList.removeAt(i);
            updatePicList();
        });

        QListWidgetItem *item = new QListWidgetItem(ui->pic_wid);
        item->setSizeHint(QSize(110, 130));
        ui->pic_wid->addItem(item);
        ui->pic_wid->setItemWidget(item, itemWidget);
    }
}
