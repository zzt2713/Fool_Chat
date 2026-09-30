#ifndef DYNAMIC_CARD_H
#define DYNAMIC_CARD_H
/******************************************************************************
*
* @file       dynamic_card.h
* @brief      动态卡片组件 - 多列瀑布流中的单条动态卡片（小红书式：封面/正文/底栏）
*
******************************************************************************/
#include <QWidget>
#include <QList>
#include <QPixmap>
#include <QDateTime>
#include <QResizeEvent>
#include "../Chat_Comp/listitembase.h"
#include "clickedlabel.h"
#include "ElaPersonPicture.h"
class ElaText;

// 动态数据结构，存储一条动态的所有信息
struct DynamicItem {
    int dynamicId = 0;             // 动态ID
    int uid = 0;                   // 发布者UID
    QString userName = "我";       // 发布者用户名
    QString nick;                  // 发布者昵称
    QString icon;                  // 发布者头像
    QString avatarPath;            // 头像路径（为空时使用默认头像）
    QString content;               // 动态文本内容
    QList<QPixmap> images;         // 附带的图片列表
    QDateTime publishTime;         // 发布时间
    int likeCount = 0;             // 点赞数
    bool likedByMe = false;        // 当前客户端是否已点赞
};

class DynamicCard : public ListItemBase
{
    Q_OBJECT

public:
    explicit DynamicCard(QWidget *parent = nullptr);
    ~DynamicCard();

    // 设置动态数据并刷新界面显示
    void SetInfo(const DynamicItem& item);
    // 获取当前卡片存储的动态数据
    DynamicItem GetInfo() const;
    // 返回卡片尺寸建议（实际高度由容器按列宽用 totalHeightForWidth 计算）
    QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    // 按下/抬起/移出时维护 state 属性，驱动 QSS 的选中态样式
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // 构建卡片UI布局
    void buildUI();
    // 按当前宽度刷新依赖尺寸的文字（正文五行截断/昵称省略）
    void refreshTexts();
    // 刷新点赞按钮显示
    void refreshLike();
    // 将时间转换为"刚刚/x分钟前/x小时前"等友好格式
    QString timeAgo(const QDateTime& time) const;
    // 创建图片网格容器（P1 图片链路打通后启用）
    QWidget* createImageGrid(const QList<QPixmap>& images);

    DynamicItem _data;             // 当前卡片的动态数据
    int _fittedW = -1;             // 高度已适配过的宽度
    QString _fittedText;           // 高度已适配过的正文（防 resize↔refresh 重复拉高成死循环）

    ElaPersonPicture* _avatarWidget = nullptr;
    ElaText* _nameLabel = nullptr;       // 昵称（底栏）
    ElaText* _timeLabel = nullptr;       // 发布时间（底栏）
    ElaText* _contentLabel = nullptr;    // 正文（最多两行）
    QWidget* _imageGridWidget = nullptr; // 图片网格容器
    ClickedLabel* _likeLabel = nullptr;  // 点赞
};

#endif // DYNAMIC_CARD_H
