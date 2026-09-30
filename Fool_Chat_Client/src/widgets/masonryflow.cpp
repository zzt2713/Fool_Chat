#include "masonryflow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>

namespace {
    // （卡片的封面/正文均为定高或已按宽度截断，此值与实际占位一致）
    int contentHeightAt(QWidget *widget, int width)
    {
        if (widget && widget->layout()) {
            return widget->layout()->totalHeightForWidth(width);
        }
        return widget ? widget->sizeHint().height() : 0;
    }
} // namespace

MasonryFlow::MasonryFlow(QWidget *parent)
    : ElaScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
    setStyleSheet("MasonryFlow { background: transparent; border: none; }");
    // 显式贴顶：内容少时不许在视口里垂直居中
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    _container = new QWidget(this);
    _container->setAutoFillBackground(false);
    setWidget(_container);

    auto *containerLayout = new QVBoxLayout(_container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    containerLayout->setAlignment(Qt::AlignTop);

    // 空态占位区：无条目时水平居中显示
    _emptyHolder = new QWidget(_container);
    _emptyHolder->setAutoFillBackground(false);
    auto *emptyLayout = new QVBoxLayout(_emptyHolder);
    emptyLayout->setContentsMargins(0, 48, 0, 48);
    emptyLayout->setAlignment(Qt::AlignHCenter);
    emptyLayout->setSpacing(0);
    _emptyHolder->setVisible(false);
    containerLayout->addWidget(_emptyHolder);

    // 多列宿主：占满容器剩余空间，内容永远从顶部开始往下排
    _columnsHost = new QWidget(_container);
    _columnsHost->setAutoFillBackground(false);
    _columnsHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    _rowLayout = new QHBoxLayout(_columnsHost);
    _rowLayout->setContentsMargins(0, 4, 14, 20);
    _rowLayout->setSpacing(_spacing);
    _rowLayout->setAlignment(Qt::AlignTop);
    containerLayout->addWidget(_columnsHost);
}

int MasonryFlow::calcColumnCount(int usableWidth) const
{
    if (usableWidth <= 0) {
        return 1;
    }
    return qMax(1, (usableWidth + _spacing) / (_minCardWidth + _spacing));
}

int MasonryFlow::columnCount() const
{
    return _columnCount;
}

void MasonryFlow::setEmptyWidget(QWidget *widget)
{
    if (!widget || !_emptyHolder) {
        return;
    }
    _emptyWidget = widget;
    _emptyHolder->layout()->addWidget(widget);
    updateEmptyVisible();
}

void MasonryFlow::updateEmptyVisible()
{
    if (_emptyHolder) {
        _emptyHolder->setVisible(_items.isEmpty() && _emptyWidget != nullptr);
    }
}

void MasonryFlow::clear()
{
    qDeleteAll(_items);
    _items.clear();
    updateEmptyVisible();
}

void MasonryFlow::refreshLayout()
{
    reflow();
    relayoutItems();
}

void MasonryFlow::addWidget(QWidget *widget)
{
    if (!widget) {
        return;
    }
    if (_columnCount <= 0) {
        reflow();
        if (_columnCount <= 0) {
            return;
        }
    }

    widget->installEventFilter(this);
    _items.append(widget);

    widget->setFixedWidth(_columnWidth);
    // 显式锁定高度=布局实测值：交给策略猜可能被拉伸，与卡片内部适配形成闭环
    widget->setFixedHeight(contentHeightAt(widget, _columnWidth));

    // 行优先：按插入顺序轮流分发，阅读顺序与数据顺序一致
    const int idx = (_items.size() - 1) % _columnCount;
    auto *lay = qobject_cast<QVBoxLayout *>(_columnBoxes[idx]->layout());
    if (lay) {
        lay->addWidget(widget);
    }
    updateEmptyVisible();
}

void MasonryFlow::removeWidget(QWidget *widget)
{
    if (!widget || !_items.removeOne(widget)) {
        return;
    }
    widget->removeEventFilter(this);
    widget->setParent(nullptr);
    widget->deleteLater();
    relayoutItems();
    updateEmptyVisible();
}

void MasonryFlow::reflow()
{
    int usable = viewport()->width()
        - _rowLayout->contentsMargins().left()
        - _rowLayout->contentsMargins().right();
    if (usable <= 60) {
        // 窗口尚未显示或视口异常：先按最小卡宽兜底，稍后 resizeEvent 会重排
        usable = _minCardWidth;
    }

    const int cols = calcColumnCount(usable);
    _columnWidth = (usable - (cols - 1) * _spacing) / cols;

    if (cols != _columnCount) {
        // 先把条目从旧列里摘出，避免删除列宿主时误删条目
        for (auto *item : _items) {
            for (auto *box : _columnBoxes) {
                if (box->layout()) {
                    box->layout()->removeWidget(item);
                }
            }
            item->setParent(nullptr);
        }
        while (_columnBoxes.size() > cols) {
            delete _columnBoxes.takeLast();
        }
        while (_columnBoxes.size() < cols) {
            auto *box = new QWidget(_columnsHost);
            box->setAutoFillBackground(false);
            auto *lay = new QVBoxLayout(box);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->setSpacing(_spacing);
            _rowLayout->addWidget(box);
            // 显式顶对齐：QHBoxLayout 默认把子项拉伸成等高（居中起点不一），第一行必须顶齐
            _rowLayout->setAlignment(box, Qt::AlignTop);
            _columnBoxes.append(box);
        }
        _columnCount = cols;
    }
    relayoutItems();
}

void MasonryFlow::relayoutItems()
{
    if (_columnBoxes.isEmpty() || _columnWidth <= 0) {
        return;
    }

    for (int i = 0; i < _items.size(); ++i) {
        QWidget *item = _items[i];
        item->setFixedWidth(_columnWidth);
        // 显式锁定高度=布局实测值（窗口变化重排时同步刷新）
        item->setFixedHeight(contentHeightAt(item, _columnWidth));

        // 行优先：第 i 条固定进第 i%n 列，重排前后顺序一致
        const int idx = i % _columnCount;
        auto *lay = qobject_cast<QVBoxLayout *>(_columnBoxes[idx]->layout());
        if (lay) {
            // QLayout 同一时刻只归属一个布局，加入新列会自动从旧列移除
            lay->addWidget(item);
        }
    }
}

void MasonryFlow::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    reflow();
}

bool MasonryFlow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *item = qobject_cast<QWidget *>(watched);
        if (item && _items.contains(item)) {
            emit itemClicked(item);
        }
    }
    return QScrollArea::eventFilter(watched, event);
}
