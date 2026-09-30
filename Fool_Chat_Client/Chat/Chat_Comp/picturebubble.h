#ifndef PICTUREBUBBLE_H
#define PICTUREBUBBLE_H
#include "bubbleframe.h"
#include <QHBoxLayout>
#include <QPixmap>

class PictureBubble : public BubbleFrame
{
    Q_OBJECT
public:
    PictureBubble(const QPixmap &picture,ChatRole role,QWidget*parent = nullptr) ;
    QPixmap picture() const { return _pix; }

signals:
    // 点击图片打开查看器
    void sigOpenImage(const QPixmap& pix);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QPixmap _pix;
};

#endif // PICTUREBUBBLE_H
