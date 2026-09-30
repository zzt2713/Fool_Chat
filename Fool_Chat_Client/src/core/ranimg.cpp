#include "ranimg.h"
#include <QPainter>

ranImg *ranImg::instance()
{
    static ranImg _instance;
    return &_instance;
}

ranImg::ranImg() {

}

// 内部方法：获取网络图片数据
QByteArray ranImg::fetchImageData(const QString& type)
{
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QNetworkRequest request;

    if(type == "pc")
        request.setUrl(QUrl("https://www.loliapi.com/acg/pc/"));
    else if(type == "pp")
        request.setUrl(QUrl("https://www.loliapi.com/acg/pp/"));
    else
        request.setUrl(QUrl("https://www.loliapi.com/acg/pe/"));

    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = manager->get(request);

    QEventLoop loop;
    QTimer timeoutTimer;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeoutTimer.setSingleShot(true);
    timeoutTimer.start(5000);

    loop.exec();

    QByteArray imageData;

    if (reply->error() == QNetworkReply::NoError && timeoutTimer.isActive()) {
        imageData = reply->readAll();
    } else {
        qWarning() << "Network error or timeout:" << reply->errorString();
    }

    reply->deleteLater();
    manager->deleteLater();

    return imageData;
}

QPixmap ranImg::getImg(QString type)
{
    QByteArray imageData = fetchImageData(type);
    QPixmap pixmap;

    if (!imageData.isEmpty()) {
        if (!pixmap.loadFromData(imageData)) {
            qWarning() << "Failed to load image from network data";
            pixmap = createDefaultPixmap();
        }
    } else {
        pixmap = createDefaultPixmap();
    }

    return pixmap;
}

QPixmap ranImg::getImg(int width, int height)
{
    QPixmap rawPix = this->getImg();
    if (rawPix.isNull()) {
        return rawPix;
    }

    return rawPix.scaled(width, height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    //IgnoreAspectRatio 允许图片变形
}

QPixmap ranImg::getImg(QSize size)
{
    return this->getImg(size.width(), size.height());
}

// 新增：返回QImage的方法
QImage ranImg::getImage(QString type)
{
    QByteArray imageData = fetchImageData(type);
    QImage image;

    if (!imageData.isEmpty()) {
        if (!image.loadFromData(imageData)) {
            qWarning() << "Failed to load image from network data";
            image = createDefaultImage();
        }
    } else {
        image = createDefaultImage();
    }

    return image;
}

QImage ranImg::getImage(int width, int height)
{
    QImage rawImage = this->getImage();
    if (rawImage.isNull()) {
        return rawImage;
    }

    return rawImage.scaled(width, height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
}

QImage ranImg::getImage(QSize size)
{
    return this->getImage(size.width(), size.height());
}

ranImg::~ranImg()
{

}

QPixmap ranImg::createDefaultPixmap()
{
    QPixmap pixmap(800, 600);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    QLinearGradient gradient(0, 0, 0, pixmap.height());
    gradient.setColorAt(0, QColor(30, 32, 40));
    gradient.setColorAt(1, QColor(50, 52, 60));
    painter.fillRect(pixmap.rect(), gradient);

    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, "Random Image Placeholder");

    return pixmap;
}

QImage ranImg::createDefaultImage()
{
    return createDefaultPixmap().toImage();
}
