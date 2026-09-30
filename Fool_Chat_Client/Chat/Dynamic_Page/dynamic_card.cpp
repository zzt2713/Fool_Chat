#include "dynamic_card.h"
#include "ElaText.h"
#include "ElaIcon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QFontMetrics>
#include <QApplication>
#include "ElaPersonPicture.h"

DynamicCard::DynamicCard(QWidget *parent)
    : ListItemBase(parent)
{
    SetItemType(ListItemType::DYNAMIC_ITEM);
    buildUI();
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

DynamicCard::~DynamicCard()
{

}

void DynamicCard::buildUI()
{
    setObjectName("DynamicCard");
    setProperty("state", "normal");
    setStyleSheet(
        "QWidget#DynamicCard {"
        "  background: rgba(128, 128, 128, 0.14);"
        "  border: 1px solid rgba(128, 128, 128, 0.25);"
        "  border-radius: 12px;"
        "}"
        "QWidget#DynamicCard:hover {"
        "  background: rgba(128, 128, 128, 0.22);"
        "  border: 1px solid rgba(128, 128, 128, 0.4);"
        "}"
        "QWidget#DynamicCard[state='press'] {"
        "  background: rgba(128, 128, 128, 0.3);"
        "  border: 1px solid rgba(128, 128, 128, 0.5);"
        "}"
    );

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // ===== 正文：内容主体，最多五行超出省略 =====
    _contentLabel = new ElaText();
    _contentLabel->setTextPixelSize(13);
    _contentLabel->setWordWrap(true);
    mainLayout->addWidget(_contentLabel);

    // 图片网格占位（P1 图片链路打通后启用）
    _imageGridWidget = nullptr;

    // 行量化等高时的富余空间压在这里：底栏始终贴卡片底部
    mainLayout->addStretch(1);

    // ===== 底栏：头像 + (昵称/时间) + 点赞 =====
    auto* footerLayout = new QHBoxLayout();
    footerLayout->setSpacing(8);

    _avatarWidget = new ElaPersonPicture(this);
    _avatarWidget->setPictureSize(20);
    footerLayout->addWidget(_avatarWidget);

    auto* nameTimeLayout = new QVBoxLayout();
    nameTimeLayout->setSpacing(1);
    nameTimeLayout->setContentsMargins(0, 0, 0, 0);

    _nameLabel = new ElaText();
    _nameLabel->setTextPixelSize(12);
    _nameLabel->setStyleSheet("font-weight: 600;");
    nameTimeLayout->addWidget(_nameLabel);

    _timeLabel = new ElaText();
    _timeLabel->setTextPixelSize(10);
    _timeLabel->setStyleSheet("color: rgba(128, 128, 128, 0.9);");
    nameTimeLayout->addWidget(_timeLabel);

    footerLayout->addLayout(nameTimeLayout);
    footerLayout->addStretch();

    _likeLabel = new ClickedLabel(this);
    _likeLabel->setTextPixelSize(14);
    footerLayout->addWidget(_likeLabel);

    // 本地点赞态：P0 仅做 UI 反馈（P2 接服务端接口后改为持久化）
    connect(_likeLabel, &ClickedLabel::clicked, this, [this](QString, ClickLbState) {
        _data.likedByMe = !_data.likedByMe;
        _data.likeCount += _data.likedByMe ? 1 : -1;
        refreshLike();
    });

    mainLayout->addLayout(footerLayout);
}

// 设置动态数据并更新所有UI元素
void DynamicCard::SetInfo(const DynamicItem& item)
{
    _data = item;

    // 头像
    if (!item.avatarPath.isEmpty()) {
        QPixmap avatar(item.avatarPath);
        if (!avatar.isNull()) {
            _avatarWidget->setPicture(avatar);
        } else {
            _avatarWidget->setDisplayName(item.userName);
        }
    } else {
        _avatarWidget->setDisplayName(item.userName);
    }
    _avatarWidget->setPictureSize(20);

    refreshLike();
    refreshTexts();

    // 清除旧的图片网格
    if (_imageGridWidget) {
        _imageGridWidget->deleteLater();
        _imageGridWidget = nullptr;
    }
    if (!item.images.isEmpty()) {
        _imageGridWidget = createImageGrid(item.images);
        // 插入到底栏之前
        auto* mainLayout = qobject_cast<QVBoxLayout*>(layout());
        // 插在正文之后、弹性空隙之前（count-2：末位是 footer，倒数二是 stretch）
        mainLayout->insertWidget(mainLayout->count() - 2, _imageGridWidget);
    }
}

DynamicItem DynamicCard::GetInfo() const
{
    return _data;
}

// 按当前宽度刷新所有依赖尺寸的文字
void DynamicCard::refreshTexts()
{
    if (!_contentLabel || !_nameLabel || !_timeLabel) {
        return;
    }
    const int w = width();
    if (w <= 0) {
        return;
    }
    const int innerW = qMax(80, w - 20); // 左右各10内边距

    // 正文：最多五行超出省略
    QString labelText;
    int fitLines = 0;
    if (_data.content.trimmed().isEmpty()) {
        labelText.clear();
    } else {
        const int maxLines = 5;
        const QFontMetrics bodyFm(_contentLabel->font());
        const int lineH = bodyFm.lineSpacing();
        const QRect need = bodyFm.boundingRect(QRect(0, 0, innerW, 1 << 20),
                                               Qt::TextWordWrap, _data.content);
        const int estLines = (need.height() + lineH - 1) / lineH;
        labelText = _data.content;
        if (estLines > maxLines) {
            labelText = bodyFm.elidedText(_data.content, Qt::ElideRight, int(innerW * (maxLines - 0.1)));
        }
        fitLines = qBound(1, estLines, maxLines);
    }

    // 昵称按剩余宽度省略（纯展示，不参与高度）
    const int nameW = qMax(40, innerW - 20 - 46 - 20);
    const QFontMetrics nameFm(_nameLabel->font());
    _nameLabel->setText(nameFm.elidedText(_data.userName, Qt::ElideRight, nameW));
    _timeLabel->setText(timeAgo(_data.publishTime));

    if (w == _fittedW && labelText == _fittedText) {
        return;
    }
    _fittedW = w;
    _fittedText = labelText;

    if (labelText.isEmpty()) {
        _contentLabel->setVisible(false);
        _contentLabel->setFixedHeight(0);
        return;
    }
    _contentLabel->setVisible(true);
    _contentLabel->setText(labelText);
    const QFontMetrics bodyFm(_contentLabel->font());
    const int lineH = bodyFm.lineSpacing();
    int h = _contentLabel->heightForWidth(innerW);
    if (h <= 0) {
        h = bodyFm.height() + (fitLines - 1) * lineH;
    }
    _contentLabel->setFixedHeight(h + 2);
}

// 刷新点赞按钮显示
void DynamicCard::refreshLike()
{
    _likeLabel->setText(QString("%1 %2").arg(_data.likedByMe ? "♥" : "♡").arg(_data.likeCount));
    // 悬停给红色反馈，暗示可点击（真点赞接口是 P2）
    _likeLabel->setStyleSheet(_data.likedByMe
        ? "color: #ff2442; font-weight: 600; QLabel:hover { color: #e0123a; }"
        : "color: rgba(128, 128, 128, 0.9); font-weight: 400; QLabel:hover { color: #ff2442; }");
}

// 列宽变化时重刷按宽度截断的文字
void DynamicCard::resizeEvent(QResizeEvent* event)
{
    ListItemBase::resizeEvent(event);
    if (width() <= 0) {
        return;
    }
    refreshTexts();
}

void DynamicCard::mousePressEvent(QMouseEvent* event)
{
    setProperty("state", "press");
    repolish(this);
    // 基类默认忽略，不阻断容器的点击过滤器
    ListItemBase::mousePressEvent(event);
}

void DynamicCard::mouseReleaseEvent(QMouseEvent* event)
{
    setProperty("state", "normal");
    repolish(this);
    ListItemBase::mouseReleaseEvent(event);
}

void DynamicCard::leaveEvent(QEvent* event)
{
    // 按下后拖出卡片再抬起：离开时兜底复位，避免状态卡在 press
    if (property("state").toString() == "press") {
        setProperty("state", "normal");
        repolish(this);
    }
    ListItemBase::leaveEvent(event);
}

// 卡片高度交给布局按当前宽度计算（容器用 totalHeightForWidth 取值）
QSize DynamicCard::sizeHint() const
{
    return layout() ? layout()->sizeHint() : QWidget::sizeHint();
}

// 将时间戳转换为友好的相对时间描述
QString DynamicCard::timeAgo(const QDateTime& time) const
{
    if (!time.isValid()) return "刚刚";

    qint64 secs = time.secsTo(QDateTime::currentDateTime());
    if (secs < 60) return "刚刚";
    if (secs < 3600) return QString("%1分钟前").arg(secs / 60);
    if (secs < 86400) return QString("%1小时前").arg(secs / 3600);
    if (secs < 604800) return QString("%1天前").arg(secs / 86400);
    return time.toString("MM-dd HH:mm");
}

// 创建图片网格，每行最多3张，每张80x80带圆角
QWidget* DynamicCard::createImageGrid(const QList<QPixmap>& images)
{
    auto* container = new QWidget();
    container->setStyleSheet("background: transparent; border: none;");
    auto* gridLayout = new QGridLayout(container);
    gridLayout->setSpacing(6);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    int maxSize = 80;
    int cols = qMin(images.size(), 3);

    for (int i = 0; i < images.size(); ++i) {
        QLabel* imgLabel = new QLabel();
        imgLabel->setFixedSize(maxSize, maxSize);
        imgLabel->setScaledContents(true);
        imgLabel->setStyleSheet("border-radius: 6px; border: 1px solid rgba(128, 128, 128, 0.25);");
        // 等比缩放并裁剪填充
        QPixmap scaled = images[i].scaled(maxSize, maxSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        imgLabel->setPixmap(scaled);
        gridLayout->addWidget(imgLabel, i / cols, i % cols);
    }

    return container;
}
