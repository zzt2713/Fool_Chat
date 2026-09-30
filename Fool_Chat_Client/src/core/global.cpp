#include "global.h"

QString gate_url_prefix = "";
QString NAME = "";
QString EMAIL = "";

std::function<void(QWidget*)> repolish = [](QWidget* w){
    w->style()->unpolish(w);
    w->style()->polish(w);
};

std::function<QString(QString)> hashString = [](QString input){
    // 计算哈希值
    QByteArray data = input.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);

    // 转换为十六进制字符串
    return QString(hash.toHex());
};
