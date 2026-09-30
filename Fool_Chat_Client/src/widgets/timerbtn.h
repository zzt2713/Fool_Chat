#ifndef TIMERBTN_H
#define TIMERBTN_H
/******************************************************************************
*
* @file       timerbtn.h
* @brief      倒计时按钮
*
* @author     Fool
* @date       2026/02/24
* @history
*****************************************************************************/

#include <QPushButton>
#include <QTimer>

class TimerBtn : public QPushButton
{
public:
    TimerBtn(QWidget * parent = nullptr);
    ~TimerBtn();
    void mouseReleaseEvent(QMouseEvent *e) override;
    void stopCountdown();
private:
    QTimer *_timer;
    int _counter;
};

#endif // TIMERBTN_H
