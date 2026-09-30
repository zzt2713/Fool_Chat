#ifndef NOTIFYPOPUP_H
#define NOTIFYPOPUP_H

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QWidget>

class NotifyCard;
class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QTimer;

// 右下角通知弹窗控制器：维护卡片垂直队列、右下角锚定堆叠、点击回传主窗口跳转
class NotifyPopup : public QObject
{
    Q_OBJECT
public:
    // 通知种类：决定卡片点击后主窗口的跳转目标
    enum Kind
    {
        TextChat = 0, // 新聊天消息 → 打开对应会话
        FriendApply,  // 好友申请 → 通讯录申请页
        AuthFriend,   // 对方通过我的申请 → 打开与新好友的会话
        IncomingCall  // 视频来电 → 唤起来电弹窗
    };

    // parent 须为主窗口（C_Window），用作屏幕定位锚点，析构随主窗口
    explicit NotifyPopup(QObject* parent = nullptr);
    ~NotifyPopup() override;

    // 弹出一张卡片；标题/正文由调用方拼好，控制器不关心业务语义
    void notify(int kind, int uid, const QString& title,
                const QString& body, const QString& iconPath);

Q_SIGNALS:
    // 卡片被点击；kind/uid 原样回传，跳转逻辑由 C_Window 实现
    Q_SIGNAL void sigNoticeClicked(int kind, int uid);

private:
    // 卡片超上限时移除最旧一张（无动画，避免挤出时抖动）
    void evictOverflow();
    // "最新在最底"重算各卡目标坐标，已显示的卡平移到新位置
    void relayout();
    // 卡片消失（超时/被点击）后的收尾：出队 + 销毁 + 补位
    void removeCard(NotifyCard* card);
    // 加载头像，失败回退到默认资源图
    QPixmap loadAvatar(const QString& iconPath) const;

    QWidget* _anchor{nullptr}; // 主窗口锚点，仅用于取屏幕，不作父窗口
    QList<NotifyCard*> _cards; // 自底向上：index 0 = 最新（最靠屏幕底边）
};

// 单张通知卡片：无边框置顶小窗，滑入 + 淡入 + 自动消失 + 悬停暂停 + 可点击
class NotifyCard : public QWidget
{
    Q_OBJECT
public:
    NotifyCard(int kind, int uid, const QString& title, const QString& body,
               const QPixmap& avatar, QWidget* parent = nullptr);

    // 移动到堆叠新位置（隐藏时直接 move，显示中用动画平移）
    void moveTo(const QPoint& targetPos);
    // 右侧滑入 + 淡入（须在 show() 之后调用，否则动画不执行）
    void startEntrance();
    // 可合并：同 kind+uid 且未在淡出中（同人连发复用本卡）
    bool canMerge(int kind, int uid) const;
    // 刷新卡片内容并重置倒计时（合并复用时调用）
    void refresh(const QString& title, const QString& body);

Q_SIGNALS:
    // 卡片被点击（含 kind/uid，控制器转发给主窗口）
    Q_SIGNAL void clicked(int kind, int uid);
    // 卡片消失完毕（超时/点击后淡出结束），控制器据此出队补位
    Q_SIGNAL void expired(NotifyCard* self);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // 淡出后发 expired（超时与点击共用）
    void startDismiss();

    int _kind{0};              // NotifyPopup::Kind，点击回传用
    int _uid{0};               // 对方/申请者 uid，点击回传用
    QString _title;            // 昵称标题行
    QString _body;             // 消息预览/说明正文（进入时已 elide）
    QPixmap _avatar;           // 圆形裁剪后的头像
    int _count{1};             // 合并的消息条数（同人连发累计，标题展示用）
    QTimer* _dismissTimer{nullptr}; // 自动消失倒计时（悬停暂停）
    int _remaining{5000};      // 剩余毫秒，悬停期间不递减
    bool _hovered{false};      // 悬停中（暂停倒计时）
    bool _dismissing{false};   // 淡出中，防止重复触发 expired
};

#endif // NOTIFYPOPUP_H
