#include "ranimg.h"
#include <QPainter>

namespace {
// 图床地址：pc 电脑壁纸 / pp 头像 / pe 移动壁纸，其余按 pc 处理
QUrl ImageUrlFor(const QString& type)
{
    if (type == "pp") {
        return QUrl("https://www.loliapi.com/acg/pp/");
    }
    if (type == "pe") {
        return QUrl("https://www.loliapi.com/acg/pe/");
    }
    return QUrl("https://www.loliapi.com/acg/pc/");
}

void FillImageRequest(QNetworkRequest& request, const QUrl& url)
{
    request.setUrl(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
}
} // namespace

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
    FillImageRequest(request, ImageUrlFor(type));

    // 图床偶发超时/限流：失败自动重试一次
    QByteArray imageData;
    for (int attempt = 0; attempt < 2 && imageData.isEmpty(); ++attempt) {
        QNetworkReply* reply = manager->get(request);

        QEventLoop loop;
        QTimer timeoutTimer;

        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

        timeoutTimer.setSingleShot(true);
        timeoutTimer.start(5000);

        loop.exec();

        if (reply->error() == QNetworkReply::NoError && timeoutTimer.isActive()) {
            imageData = reply->readAll();
        } else {
            qWarning() << "Network error or timeout:" << reply->errorString();
        }

        reply->deleteLater();
    }

    manager->deleteLater();

    return imageData;
}

QPixmap ranImg::getImgOrNull(const QString& type)
{
    const QByteArray imageData = fetchImageData(type);
    if (imageData.isEmpty()) {
        return QPixmap();
    }
    QPixmap pixmap;
    if (!pixmap.loadFromData(imageData)) {
        return QPixmap();
    }
    return pixmap;
}

void ranImg::getImgOrNullAsync(const QString& type,
                               const std::function<void(QPixmap)>& cb,
                               int retriesLeft)
{
    QNetworkRequest request;
    FillImageRequest(request, ImageUrlFor(type));
    // 网络层超时兜底，不再依赖嵌套事件循环计时
    request.setTransferTimeout(8000);

    QNetworkReply* reply = _manager.get(request);
    // reply 作接收者：请求对象销毁时连接自动断开，回调内不触碰单例生命周期
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [this, reply, cb, retriesLeft, type]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        const QString errStr = reply->errorString();
        QByteArray data = ok ? reply->readAll() : QByteArray();
        reply->deleteLater();

        if (!ok) {
            qWarning() << "Async image fetch failed, retry left "
                       << retriesLeft - 1 << ":" << errStr;
            if (retriesLeft > 0) {
                getImgOrNullAsync(type, cb, retriesLeft - 1);
                return;
            }
            cb(QPixmap());
            return;
        }

        QPixmap pixmap;
        if (!pixmap.loadFromData(data)) {
            pixmap = QPixmap();
        }
        cb(pixmap);
    });
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
