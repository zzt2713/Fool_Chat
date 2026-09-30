#ifndef FLOATINGTIP_H
#define FLOATINGTIP_H

/**
 * @brief  错误信息显示
 * 包含文件 #include "FloatingTip.h" 弹出Tip提示框
 * ShowTip(this,str,b_ok)
 * this 父窗口
 * str 提示字
 * b_ok true 绿色成功 false 红色警告框
 *
**/

#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include "../core/F_singleton.h"

#define ShowTip(pa,str,b_ok) FloatingTip::showTip(pa, str, b_ok);

class FloatingTip : public QWidget, public F_Singleton<FloatingTip>
{
    friend class F_Singleton<FloatingTip>;
    Q_OBJECT

public:
    static void showTip(QWidget *parent, const QString &text, bool isSuccess = true);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    explicit FloatingTip();
    bool _isHoveredCloseBtn = false;

    void resetContent(QWidget *parent, const QString &text, bool isSuccess);
    void startEntranceAnimation();

    bool _isSuccess;
    QString _text;

    QTimer *_animTimer;
    int _totalTime = 3000;
    int _curTime = 3000;

    QRect _closeBtnRect;
    bool _isHovered = false;
};

#endif // FLOATINGTIP_H
