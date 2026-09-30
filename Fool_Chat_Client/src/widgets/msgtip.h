#ifndef MSGTIP_H
#define MSGTIP_H
/**
 * @file msgTip.h
 * @brief 消息提示框单例类封装，基于 ElaMessageBar 实现四种提示风格
 * @code
 * // 错误提示（type=0）
 * ADDMSG(ElaMessageBarType::TopRight, "操作失败", this, 0);
 * // 成功提示（type=1）
 * ADDMSG(ElaMessageBarType::Bottom, "操作成功", this, 1);
 * // 信息提示（type=2）
 * ADDMSG(ElaMessageBarType::TopLeft, "这是一条信息", this, 2);
 * // 警告提示（type=3）
 * ADDMSG(ElaMessageBarType::TopRight, "警告信息", this, 3);
 * // 带自定义显示时长（2000ms）的提示
 * ADDMSG(ElaMessageBarType::TopRight, "自定义时长", this, 0, 2000);
 * 位置参数：
 *  Top = 0x0000,
    Left = 0x0001,
    Bottom = 0x0002,
    Right = 0x0003,
    TopRight = 0x0004,
    TopLeft = 0x0005,
    BottomRight = 0x0006,
    BottomLeft = 0x0007,
 * 类型参数：
 *  0: 错误 (error)
 *  1: 成功 (success)
 *  2: 信息 (info)
 *  3: 警告 (warning)
 */

#include "ElaMessageBar.h"
#include <QQueue>
#include <QTimer>
#include <QPointer>
#include <QList>
#include <QMutex>
#include <QApplication>
#include <QDebug>

#define ADDMSG(...)    msgTip::GetInstance()->addMsg(__VA_ARGS__)
#define CLEAR          msgTip::GetInstance()->clear()

struct MsgData {
    ElaMessageBarType::PositionPolicy policy;
    QString text;
    QPointer<QWidget> parent;
    int type;
    int duration;
};

class msgTip : public QObject
{
    Q_OBJECT
public:
    static msgTip* GetInstance();

    ~msgTip();
    void addMsg(ElaMessageBarType::PositionPolicy policy, QString text, QWidget* parent, int type, int duration = 1000);

    void clear();

private:
    static msgTip* _instance;
    msgTip();
    void processQueue();

    QQueue<MsgData> _msgQueue;
    QTimer* _consumerTimer;
    std::atomic<bool> _isClearing;
};
#endif // MSGTIP_H
