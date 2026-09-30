#include "dynamic_page.h"
#include "ui_dynamic_page.h"
#include "ElaFloatButton.h"
#include "ElaMenu.h"
#include "ElaText.h"
#include "dynamic_detail.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QDebug>
#include "tcpmgr.h"
#include <QJsonDocument>
#include "usermgr.h"
#include "ElaScrollBar.h"
#include "ElaToolButton.h"
#include "msgtip.h"

Dynamic_Page::Dynamic_Page(QWidget *parent)
    : Page_Base(parent)
    , ui(new Ui::Dynamic_Page)
{
    // ElaScrollPage 自带内部布局，页面 UI 挂到滚动区内容上
    auto* content = new QWidget();
    ui->setupUi(content);
    addCentralWidget(content);
    this->setTitleVisible(false);
    setWindowTitle("心情树洞");
    // 页面与列表背景透明：暗黑主题下不留白块
    // 类名限定只作用于本页，避免级联污染子控件（ElaFloatButton 自绘会出重影）
    setStyleSheet("Dynamic_Page { background: transparent; }");
    ui->listWidget->viewport()->setStyleSheet("background: transparent;");

    // 自定义滚动条（MasonryFlow 继承 QScrollArea，接口兼容）
    ElaScrollBar* customScrollBar = new ElaScrollBar(this);
    ui->listWidget->setVerticalScrollBar(customScrollBar);
    ui->listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 空态占位：列表无内容时由 MasonryFlow 自动居中显示
    auto* emptyWidget = new QWidget();
    auto* emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    auto* emptyText = new ElaText("暂无动态，点击右下角发布第一条");
    emptyText->setTextPixelSize(13);
    emptyText->setStyleSheet("color: rgba(128, 128, 128, 0.9);");
    emptyLayout->addWidget(emptyText);
    ui->listWidget->setEmptyWidget(emptyWidget);

    // 标题行右侧加刷新按钮：重新拉取动态列表
    auto* refresh_btn = new ElaToolButton(this);
    refresh_btn->setIsTransparent(true);
    refresh_btn->setElaIcon(ElaIconType::ArrowsRotate);
    refresh_btn->setToolTip("刷新动态");
    refresh_btn->setFixedSize(36, 36);
    ui->horizontalLayout_2->addWidget(refresh_btn);
    connect(refresh_btn, &ElaToolButton::clicked, this, [this]() {
        _refreshPending = true;
        loadDynamicList();
    });

    // 悬浮按钮菜单：发布动态
    ElaMenu* menu = new ElaMenu(this);
    QAction* publishAction = menu->addAction("发布动态");
    ui->floatBtn->setMenu(menu);

    // 点击"发布动态"后打开发布弹框
    connect(publishAction, &QAction::triggered, this, [this]() {
        NewStar* content = new NewStar(this);
        // 监听发布信号，收到后通过网络发送到服务器
        connect(content, &NewStar::sigPublish, this, [this](const QString& text, const QList<QPixmap>& images) {
            QJsonObject jsonObj;
            jsonObj["uid"] = UserMgr::GetInstance()->GetUid();
            jsonObj["content"] = text;
            QJsonDocument doc(jsonObj);
            emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_PUBLISH_DYNAMIC_REQ, doc.toJson());
        });
        content->show();
    });

    // 连接动态列表响应信号
    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_dynamic_list, this, [this](QJsonArray dynamicList) {
        // 刷新按钮触发的这次回包：列表已到即算刷新成功
        if (_refreshPending) {
            _refreshPending = false;
            ADDMSG(ElaMessageBarType::Top, "刷新成功", this, 1, 2000);
        }
        _dynamicList.clear();
        ui->listWidget->clear();

        if (dynamicList.isEmpty()) {
            return;
        }

        for (int i = 0; i < dynamicList.size(); ++i) {
            const auto item = dynamicList.at(i);
            QJsonObject obj = item.toObject();
            DynamicItem dynamicItem;
            dynamicItem.dynamicId = obj["id"].toInt();
            dynamicItem.uid = obj["uid"].toInt();
            dynamicItem.userName = obj["name"].toString();
            dynamicItem.nick = obj["nick"].toString();
            dynamicItem.icon = obj["icon"].toString();
            dynamicItem.avatarPath = obj["icon"].toString();
            dynamicItem.content = obj["content"].toString();
            dynamicItem.likeCount = obj["like_count"].toInt();

            dynamicItem.publishTime = QDateTime::fromString(obj["create_time"].toString(), "yyyy-MM-ddTHH:mm:ss");
            if (!dynamicItem.publishTime.isValid()) {
                dynamicItem.publishTime = QDateTime::fromString(obj["create_time"].toString(), "yyyy-MM-dd HH:mm:ss");
            }
            _dynamicList.append(dynamicItem);
            addDynamicCard(dynamicItem);
        }

        // 批量插入后统一重排：首帧视口宽度未生效时卡片会挤成一团
        ui->listWidget->refreshLayout();
        QTimer::singleShot(0, ui->listWidget, [this]() { ui->listWidget->refreshLayout(); });
    });

    connect(TcpMgr::GetInstance().get(), &TcpMgr::sig_publish_success, this, [this]() {
        QTimer::singleShot(200, this, [this]() {
            loadDynamicList();
        });
    });

    // 悬浮按钮点击弹出菜单（定位在按钮上方）
    connect(ui->floatBtn, &ElaFloatButton::clicked, this, [this, menu]() {
        QPoint pos = ui->floatBtn->mapToGlobal(QPoint(-ui->floatBtn->width(), -ui->floatBtn->height()));
        menu->exec(pos);
    });

    // 点击卡片打开详情弹框
    connect(ui->listWidget, &MasonryFlow::itemClicked, this, [this](QWidget* widget) {
        DynamicCard* card = qobject_cast<DynamicCard*>(widget);
        if (card) {
            // 将卡片数据传给详情弹框
            DynamicDetail* detail = new DynamicDetail(card->GetInfo(), this);
            detail->show();
        }
    });

    // 测试数据
    // QList<DynamicItem> staticItems;
    // DynamicItem item1;
    // for(int i = 0;i < 15; ++i){
    //     item1.dynamicId = i;
    //     item1.uid = 1001;
    //     item1.userName = "小明";
    //     item1.nick = "小明同学";
    //     item1.icon = ":/res/head_5.jpg";
    //     item1.avatarPath = ":/res/head_5.jpg";
    //     item1.content = "今天天气真好，出去玩了！☀️";
    //     item1.likeCount = i;
    //     item1.publishTime = QDateTime::currentDateTime().addSecs(-3600);
    //     staticItems.append(item1);
    //     _dynamicList.append(item1);
    //     addDynamicCard(item1);
    // }
}

Dynamic_Page::~Dynamic_Page()
{
    delete ui;
}

void Dynamic_Page::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    loadDynamicList();
}

// 创建动态卡片并追加到瀑布流（列位置由容器按最矮列自动分配）
void Dynamic_Page::addDynamicCard(const DynamicItem& item)
{
    DynamicCard* card = new DynamicCard(ui->listWidget);
    card->SetInfo(item);
    ui->listWidget->addWidget(card);
}

// 从服务器加载动态列表
void Dynamic_Page::loadDynamicList()
{
    QJsonObject jsonObj;
    jsonObj["begin"] = 0;
    QJsonDocument doc(jsonObj);
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_GET_DYNAMIC_LIST_REQ, doc.toJson());
}
