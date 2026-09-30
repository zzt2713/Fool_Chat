#ifndef RANIMG_H
#define RANIMG_H
/**
 * 包含文件 #include "ranimg.h" 获取随机二次元图片
 *  IMG(str)    头像pp 电脑端pc 移动端pe
 *  IMG_SIZE(w,h)    获取等比例图片
 *  IMG_QSIZE(size)
 *  IMG_IMAGE(str)  返回QImage格式
 *  ------------------------------------------------------------------------
 * ranImg::instance()获取单例
 * ranImg::instance().getImg(type) 获取图片 参数type为 头像pp 电脑端pc 移动端pe
 * ranImg::instance().getImg(w,h) 获取等比例图片
 * ranImg::instance().getImg(Qsize(w,h))
 * ranImg::instance().getImage(type) 返回QImage格式
**/

#include <QNetworkRequest>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QEventLoop>
#include <QSize>
#include <QObject>
#include <QDebug>

#define IMG(str) ranImg::instance()->getImg(str)
#define IMG_SIZE(w,h) ranImg::instance()->getImg(w,h)
#define IMG_QSIZE(size) ranImg::instance()->getImg(size)
#define IMG_IMAGE(str) ranImg::instance()->getImage(str)

class ranImg
{
public:
    static ranImg* instance();
    ranImg();

    // 返回QPixmap的方法
    QPixmap getImg(QString type = "pc"); // pc为pc端图片 pe为移动端图片 pp 头像 默认获取pc端图片
    QPixmap getImg(int width, int height);
    QPixmap getImg(QSize size);

    // 返回QImage的方法
    QImage getImage(QString type = "pc");
    QImage getImage(int width, int height);
    QImage getImage(QSize size);

    ~ranImg();

private:
    // 内部方法：获取网络图片数据
    QByteArray fetchImageData(const QString& type);
    QPixmap createDefaultPixmap();
    QImage createDefaultImage();
};

#endif // RANIMG_H
