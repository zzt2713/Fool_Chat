#ifndef DYNAMIC_DETAIL_H
#define DYNAMIC_DETAIL_H
/******************************************************************************
*
* @file       dynamic_detail.h
* @brief      动态详情弹框 - 点击动态卡片后弹出，展示完整的动态内容
*
******************************************************************************/
#include "ElaDialog.h"
#include "dynamic_card.h"

class ElaText;
class QLabel;
class ElaToolButton;
class ClickedLabel;

class DynamicDetail : public ElaDialog
{
    Q_OBJECT

public:
    // 传入动态数据构造详情弹框
    explicit DynamicDetail(const DynamicItem& item, QWidget *parent = nullptr);
    ~DynamicDetail();

signals:
    void sigLikeChanged(int dynamicId, int likeCount, bool liked);

private:
    // 构建弹框UI布局
    void buildUI(const DynamicItem& item);
    // 创建图片网格（比卡片中显示更大）
    QWidget* createImageGrid(const QList<QPixmap>& images);
    // 格式化时间为"yyyy年MM月dd日 HH:mm"格式
    QString formatTime(const QDateTime& time) const;
    void updateLikeLabel();

    ElaToolButton* _closeBtn;      // 关闭按钮
    ClickedLabel* _likeLabel = nullptr;
    DynamicItem _item;
    bool _liked = false;
};

#endif // DYNAMIC_DETAIL_H
