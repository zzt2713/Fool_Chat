#include "timerbtn.h"
#include <QMouseEvent>
#include <QDebug>

TimerBtn::TimerBtn(QWidget *parent):QPushButton(parent),_counter(60)
{
    _timer = new QTimer(this);
    connect(_timer,&QTimer::timeout,[this](){
        _counter--;
        if(_counter <= 0){
            _timer->stop();
            _counter = 60;
            this->setText("获取");
            this->setEnabled(true);
            return;
        }
        this->setText(QString::number(_counter));
    });
}

TimerBtn::~TimerBtn()
{
    _timer->stop();
}

void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
{
    if(e->button() == Qt::LeftButton){
        this->setEnabled(false);
        this->setText(QString::number(_counter));
        _timer->start(1000);
        emit clicked();
    }
    // 调用基类
    QPushButton::mouseReleaseEvent(e);
}

void TimerBtn::stopCountdown()
{
    if (_timer->isActive()) {
        _timer->stop();
        _counter = 60;  // 重置计数器
        this->setText("获取");
        this->setEnabled(true);
    }
}
