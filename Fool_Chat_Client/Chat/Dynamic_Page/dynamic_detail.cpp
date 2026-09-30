#include "dynamic_detail.h"
#include "ElaText.h"
#include "ElaToolButton.h"
#include "ElaIcon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QApplication>


DynamicDetail::DynamicDetail(const DynamicItem& item, QWidget *parent)
    : ElaDialog(parent)
    , _item(item)
    , _liked(item.likedByMe)
{
    setWindowTitle("动态详情");
    setFixedWidth(520);
    buildUI(item);
}

DynamicDetail::~DynamicDetail()
{
}

// 构建详情弹框：头部信息 + 可滚动内容区(文本+大图) + 底部互动栏
void DynamicDetail::buildUI(const DynamicItem& item)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== 头部区域：头像 + 用户名 + 发布时间 + 关闭按钮 =====
    auto* headerWidget = new QWidget();
    headerWidget->setStyleSheet("background: transparent;");
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    headerLayout->setSpacing(10);

    // 头像
    ElaPersonPicture* avatarWidget = new ElaPersonPicture(this);
    avatarWidget->setPictureSize(42);
    if (!item.avatarPath.isEmpty()) {
        QPixmap avatar(item.avatarPath);
        if (!avatar.isNull()) {
            avatarWidget->setPicture(avatar);
        } else {
            avatarWidget->setDisplayName(item.userName);
        }
    } else {
        avatarWidget->setDisplayName(item.userName);
    }
    headerLayout->addWidget(avatarWidget);

    // 用户名和发布时间
    auto* nameTimeLayout = new QVBoxLayout();
    nameTimeLayout->setSpacing(3);
    nameTimeLayout->setContentsMargins(0, 0, 0, 0);

    ElaText* nameLabel = new ElaText(item.userName);
    nameLabel->setTextPixelSize(14);
    nameLabel->setStyleSheet("font-weight: 600;");
    nameTimeLayout->addWidget(nameLabel);

    ElaText* timeLabel = new ElaText(formatTime(item.publishTime));
    timeLabel->setTextPixelSize(11);
    timeLabel->setStyleSheet("color: rgba(128, 128, 128, 0.9);");
    nameTimeLayout->addWidget(timeLabel);

    headerLayout->addLayout(nameTimeLayout);
    headerLayout->addStretch();

    // 关闭按钮
    _closeBtn = new ElaToolButton();
    _closeBtn->setElaIcon(ElaIconType::Xmark);
    _closeBtn->setFixedSize(28, 28);
    _closeBtn->setStyleSheet("QToolButton { border: none; background: transparent; border-radius: 14px; } QToolButton:hover { background: rgba(128, 128, 128, 0.2); }");
    _closeBtn->setToolTip("关闭");
    connect(_closeBtn, &ElaToolButton::clicked, this, &DynamicDetail::accept);
    headerLayout->addWidget(_closeBtn);

    mainLayout->addWidget(headerWidget);

    // 分割线
    auto* sep1 = new QWidget();
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background: rgba(128, 128, 128, 0.25);");
    mainLayout->addWidget(sep1);

    // ===== 可滚动内容区域：文本 + 图片 =====
    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background: transparent;");
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(14);

    // 完整文本内容
    ElaText* contentText = new ElaText(item.content);
    contentText->setTextPixelSize(14);
    contentText->setWordWrap(true);
    contentLayout->addWidget(contentText);

    // 图片网格（比卡片中显示更大，150x150）
    if (!item.images.isEmpty()) {
        QWidget* imageGrid = createImageGrid(item.images);
        contentLayout->addWidget(imageGrid);
    }

    contentLayout->addStretch();
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea, 1); // stretch=1 让内容区占据剩余空间

    // 分割线
    auto* sep2 = new QWidget();
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background: rgba(128, 128, 128, 0.25);");
    mainLayout->addWidget(sep2);

    // ===== 底部互动栏：点赞 + 评论 =====
    auto* footerWidget = new QWidget();
    footerWidget->setStyleSheet("background: transparent;");
    footerWidget->setFixedHeight(46);
    auto* footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(16, 0, 16, 0);
    footerLayout->setSpacing(24);

    _likeLabel = new ClickedLabel(footerWidget);
    _likeLabel->setTextPixelSize(13);
    _likeLabel->SetState("", "", "", "", "", "");
    _likeLabel->SetCurState(_liked ? ClickLbState::Selected : ClickLbState::Normal);
    updateLikeLabel();
    connect(_likeLabel, &ClickedLabel::clicked, this, [this](const QString&, ClickLbState state) {
        bool nowLiked = state == ClickLbState::Selected;
        if (nowLiked == _liked) {
            return;
        }

        _liked = nowLiked;
        _item.likedByMe = _liked;
        _item.likeCount += _liked ? 1 : -1;
        if (_item.likeCount < 0) {
            _item.likeCount = 0;
        }

        updateLikeLabel();
        emit sigLikeChanged(_item.dynamicId, _item.likeCount, _liked);
    });
    footerLayout->addWidget(_likeLabel);
    footerLayout->addStretch();
    mainLayout->addWidget(footerWidget);

    // 根据内容动态计算弹框高度
    int totalH = 12 + 42 + 12 + 1 + 16; // 头部 + 分割线 + 内容区内边距
    QFontMetrics fm(QFont("Microsoft YaHei", 14));
    int textH = fm.boundingRect(QRect(0, 0, 488, 10000), Qt::TextWordWrap, item.content).height();
    totalH += textH + 14;
    if (!item.images.isEmpty()) {
        int rows = (item.images.size() + 2) / 3; // 每行最多3张
        totalH += rows * 150 + (rows - 1) * 8 + 14;
    }
    totalH += 1 + 46 + 16; // 分割线 + 底部栏 + 内边距
    totalH = qMin(totalH, 700); // 最大高度限制
    totalH = qMax(totalH, 300); // 最小高度保证
    resize(520, totalH);
}

QWidget* DynamicDetail::createImageGrid(const QList<QPixmap>& images)
{
    auto* container = new QWidget();
    container->setStyleSheet("background: transparent; border: none;");
    auto* gridLayout = new QGridLayout(container);
    gridLayout->setSpacing(8);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    int maxSize = 150;
    int cols = qMin(images.size(), 3);

    for (int i = 0; i < images.size(); ++i) {
        ElaText* imgLabel = new ElaText();
        imgLabel->setFixedSize(maxSize, maxSize);
        imgLabel->setScaledContents(true);
        imgLabel->setStyleSheet("border-radius: 8px; border: 1px solid rgba(128, 128, 128, 0.25);");
        QPixmap scaled = images[i].scaled(maxSize, maxSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        imgLabel->setPixmap(scaled);
        gridLayout->addWidget(imgLabel, i / cols, i % cols);
    }

    return container;
}

// 格式化时间为完整日期时间格式
QString DynamicDetail::formatTime(const QDateTime& time) const
{
    if (!time.isValid()) return "";
    return time.toString("yyyy年MM月dd日 HH:mm");
}

void DynamicDetail::updateLikeLabel()
{
    if (!_likeLabel) {
        return;
    }

    _likeLabel->setText(QString("%1 点赞 %2").arg(_liked ? "♥" : "♡").arg(_item.likeCount));
    _likeLabel->setStyleSheet(_liked
        ? "color: #e0245e; font-weight: 600;"
        : "color: rgba(128, 128, 128, 0.9); font-weight: 400;");
}
