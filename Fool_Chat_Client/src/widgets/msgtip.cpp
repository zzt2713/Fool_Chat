#include "msgtip.h"

msgTip* msgTip::_instance = nullptr;

msgTip::msgTip() : _isClearing(false)
{
    _consumerTimer = new QTimer(this);
    _consumerTimer->setInterval(400);
    connect(_consumerTimer, &QTimer::timeout, this, &msgTip::processQueue);
}

msgTip *msgTip::GetInstance()
{
    if(_instance == nullptr){
        _instance = new msgTip();
    }
    return _instance;
}

msgTip::~msgTip()
{
    clear();
    if(_consumerTimer->isActive()) _consumerTimer->stop();
}

void msgTip::clear()
{
    _isClearing = true;
    _msgQueue.clear();
    if(_consumerTimer->isActive()) _consumerTimer->stop();
    _isClearing = false;
}

void msgTip::addMsg(ElaMessageBarType::PositionPolicy policy, QString text, QWidget *parent, int type, int duration)
{

    if(_isClearing) return;

    if (_msgQueue.size() >= 3) {
        return;
    }

    MsgData data;
    data.policy = policy;
    data.text = text;
    data.parent = parent;
    data.type = type;
    data.duration = duration;
    _msgQueue.enqueue(data);

    if (!_consumerTimer->isActive()) {
        QTimer::singleShot(0, this, &msgTip::processQueue);
        _consumerTimer->start();
    }
}

void msgTip::processQueue()
{
    if (_msgQueue.isEmpty() || _isClearing) {
        _consumerTimer->stop();
        return;
    }

    MsgData data = _msgQueue.dequeue();

    QWidget* targetParent = data.parent;

    if (targetParent == nullptr || !targetParent->isVisible()) {
        targetParent = QApplication::activeWindow();
        if(!targetParent || !targetParent->isVisible()) {
            return;
        }
    }

    // 面板嵌在外壳右半区，锚到顶层窗口才不会贴着面板边缘弹
    if (QWidget* topLevel = targetParent->window()) {
        targetParent = topLevel;
    }

    switch (data.type) {
    case 0: ElaMessageBar::error(data.policy, "错误", data.text, data.duration, targetParent); break;
    case 1: ElaMessageBar::success(data.policy, "成功", data.text, data.duration, targetParent); break;
    case 2: ElaMessageBar::information(data.policy, "信息", data.text, data.duration, targetParent); break;
    case 3: ElaMessageBar::warning(data.policy, "警告", data.text, data.duration, targetParent); break;
    }
}
