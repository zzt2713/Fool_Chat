#ifndef MASONRYFLOW_H
#define MASONRYFLOW_H

#include "ElaScrollArea.h"
#include <QList>
#include <QResizeEvent>

class QHBoxLayout;
class QVBoxLayout;

/**
 * @brief 多列自适应瀑布流容器
 *
 * 按最小卡宽计算列数（窗口变化自动增减列），条目按插入顺序行优先轮流分发
 * （第 i 项进入第 i%n 列），保证从左到右、从上到下的阅读顺序与数据顺序一致；
 * 高度按各自列宽下的布局高度（totalHeightForWidth）取值，列间自然错落。
 * 另提供一个"宽条目"位（setEmptyWidget）用于空态等跨全宽的占位内容。
 */
class MasonryFlow : public ElaScrollArea
{
    Q_OBJECT
public:
    explicit MasonryFlow(QWidget *parent = nullptr);

    // 清空所有普通条目（空态占位保留，清空后若无内容会自动重新显示）
    void clear();
    // 加入普通条目，容器接管宽度与列位置
    void addWidget(QWidget *widget);
    // 移除并销毁单个条目
    void removeWidget(QWidget *widget);
    // 批量插入后统一重排（首帧视口宽度未生效时避免卡片挤成一团）
    void refreshLayout();
    // 设置空态占位（无条目时自动居中显示）
    void setEmptyWidget(QWidget *widget);
    int columnCount() const;

signals:
    // 点击普通条目（子控件未消费的鼠标释放事件会传播到条目上）
    void itemClicked(QWidget *widget);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // 依据视口宽度重算列宽/列数并重排全部条目
    void reflow();
    // 按插入顺序行优先把 _items 分配进各列（第一行顶对齐，后续卡紧跟上方卡自然下排）
    void relayoutItems();
    void updateEmptyVisible();
    int calcColumnCount(int usableWidth) const;

    QWidget *_container = nullptr;      // 滚动视口内容根
    QWidget *_emptyHolder = nullptr;    // 空态占位容器
    QWidget *_columnsHost = nullptr;    // 多列宿主
    QHBoxLayout *_rowLayout = nullptr;  // 列横向排布
    QWidget *_emptyWidget = nullptr;    // 外部传入的空态控件

    QList<QWidget *> _items;            // 普通条目（保持插入顺序）
    QList<QWidget *> _columnBoxes;      // 每列一个宿主 QWidget

    int _columnCount = 0;
    int _columnWidth = 0;
    int _spacing = 12;
    int _minCardWidth = 200;
};

#endif // MASONRYFLOW_H
