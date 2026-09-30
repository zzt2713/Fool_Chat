#ifndef PIXMAPUTIL_H
#define PIXMAPUTIL_H

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

/******************************************************************************
*
* @file       pixmaputil.h
* @brief      头像等图片的高保真圆形裁剪
*
* @author     Fool
* @date       2026/09/27
*****************************************************************************/

namespace PixmapUtil {

// 物理像素画布绘制 + devicePixelRatio 还原，避免高 DPI 下缩放发糊
inline QPixmap round(const QPixmap& src, QSize size, qreal dpr)
{
    if (src.isNull() || size.isEmpty()) {
        return src;
    }
    if (dpr <= 0) {
        dpr = 1.0;
    }

    int diameter = qMin(size.width(), size.height());
    if (diameter <= 0) {
        return src;
    }

    QSize logicalSize(diameter, diameter);
    QSize physicalSize = logicalSize * dpr;

    QPixmap scaledSrc = src.scaled(physicalSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    scaledSrc.setDevicePixelRatio(dpr);

    QPixmap roundPixmap(physicalSize);
    roundPixmap.fill(Qt::transparent);
    roundPixmap.setDevicePixelRatio(dpr);

    QPainter painter(&roundPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addEllipse(0, 0, diameter, diameter);
    painter.setClipPath(path);

    qreal x = (diameter - scaledSrc.width() / dpr) / 2.0;
    qreal y = (diameter - scaledSrc.height() / dpr) / 2.0;
    painter.drawPixmap(QPointF(x, y), scaledSrc);

    return roundPixmap;
}

}

#endif // PIXMAPUTIL_H
