#include "applyfriend.h"
#include "msgtip.h"
#include "ui_applyfriend.h"
#include "usermgr.h"
#include "tcpmgr.h"
#include <QScrollBar>
#include "../../src/widgets/msgtip.h"
#include <QJsonDocument>

ApplyFriend::ApplyFriend(QWidget *parent)
    : ElaDialog(parent)
    , ui(new Ui::ApplyFriend)
{
    ui->setupUi(this);
    setWindowTitle("添加联系人");
    // 只保留关闭按钮
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    this->setObjectName("ApplyFriend");
    this->setModal(true);
    ui->sure_btn->setAutoDefault(false);

    ui->name_ed->setPlaceholderText(tr("Fool"));
    ui->lb_ed->setPlaceholderText("搜索、添加标签");
    ui->back_ed->setPlaceholderText("燃烧的胸毛");

    ui->more_lb->setElaIcon(ElaIconType::AnglesDown);
    ui->more_lb->setToolTip("更多标签");

    ui->lb_ed->setMaxLength(10);
    ui->input_tip_wid->hide();


    _tip_cur_point = QPoint(5, 5);

    _tip_data = { "同学","家人","菜鸟教程","C++ Primer","Rust 程序设计",
                 "父与子学Python","nodejs开发指南","go 语言开发指南",
                 "游戏伙伴","金融投资","微信读书","拼多多拼友" };

    connect(ui->more_lb, &ClickedOnceLabel::clicked, this, &ApplyFriend::ShowMoreLabel);
    InitTipLbs();
    //链接输入标签回车事件
    ui->lb_ed->installEventFilter(this);
    connect(ui->lb_ed, &ElaLineEdit::returnPressed, this, &ApplyFriend::SlotLabelEnter);
    connect(ui->lb_ed, &ElaLineEdit::textChanged, this, &ApplyFriend::SlotLabelTextChange);
    connect(ui->lb_ed, &ElaLineEdit::editingFinished, this, &ApplyFriend::SlotLabelEditFinished);
    connect(ui->tip_lb, &ClickedOnceLabel::clicked, this, &ApplyFriend::SlotAddFirendLabelByClickTip);

    ui->scrollArea->horizontalScrollBar()->setHidden(true);
    ui->scrollArea->verticalScrollBar()->setHidden(true);
    ui->scrollArea->installEventFilter(this);
    //连接确认和取消按钮的槽函数
    connect(ui->cancel_btn, &QPushButton::clicked, this, &ApplyFriend::SlotApplyCancel);
    connect(ui->sure_btn, &QPushButton::clicked, this, &ApplyFriend::SlotApplySure);
}

ApplyFriend::~ApplyFriend()
{
    delete ui;
}

void ApplyFriend::InitTipLbs()
{
    int lines = 1;
    const int HORIZONTAL_SPACING = 10;  // 水平间距
    const int VERTICAL_SPACING = 10;     // 垂直间距
    const int START_X = 5;
    const int START_Y = 5;

    int currentX = START_X;
    int currentY = START_Y;

    for(int i = 0; i < static_cast<int>(_tip_data.size()); i++){
        auto* lb = new ClickedLabel(ui->lb_list);
        lb->SetState("normal", "hover", "pressed", "selected_normal",
                     "selected_hover", "selected_pressed");
        lb->setObjectName("tipslb");
        lb->setText(_tip_data[i]);
        connect(lb, &ClickedLabel::clicked, this, &ApplyFriend::SlotChangeFriendLabelByTip);

        // 先显示才能获取正确的宽度
        lb->show();

        // 获取标签的实际宽度
        int labelWidth = lb->width();
        int labelHeight = lb->height();

        // 检查是否需要换行
        if (currentX + labelWidth > ui->lb_list->width() - START_X) {
            lines++;
            if (lines > 2) {
                delete lb;
                return;
            }
            currentX = START_X;
            currentY += labelHeight + VERTICAL_SPACING;
        }

        // 设置位置
        lb->move(currentX, currentY);

        // 更新下一个标签的X坐标
        currentX += labelWidth + HORIZONTAL_SPACING;

        _add_labels.insert(lb->text(), lb);
        _add_label_keys.push_back(lb->text());
    }
}

void ApplyFriend::AddTipLbs(ClickedLabel *lb, QPoint cur_point, QPoint &next_point, int text_width, int text_height)
{
    if(_friend_labels.size() >= 3){
        ADDMSG(ElaMessageBarType::Top,"标签已达上限!",this,0);
        return;
    }
    const int HORIZONTAL_SPACING = 10;  // 水平间距

    lb->move(cur_point);
    lb->show();
    _add_labels.insert(lb->text(), lb);
    _add_label_keys.push_back(lb->text());

    next_point.setX(cur_point.x() + lb->width() + HORIZONTAL_SPACING);
    next_point.setY(cur_point.y());
}

bool ApplyFriend::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->lb_ed && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        // 如果按下的是 回车键 (大键盘回车 或 小键盘回车)
        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)
        {
            SlotLabelEnter();
            return true;
        }
    }

    if (obj == ui->scrollArea && event->type() == QEvent::Enter)
    {
        ui->scrollArea->verticalScrollBar()->setHidden(false);
    }
    else if (obj == ui->scrollArea && event->type() == QEvent::Leave)
    {
        ui->scrollArea->verticalScrollBar()->setHidden(true);
    }

    // 注意：如果是 ElaDialog，这里最好调父类的事件过滤器
    return ElaDialog::eventFilter(obj, event);
}
void ApplyFriend::SetSearchInfo(std::shared_ptr<SearchInfo> si)
{
    _si = si;
    auto applyname = UserMgr::GetInstance()->GetName();
    auto bakname = si->_name;
    ui->name_ed->setText(applyname);
    ui->back_ed->setText(bakname);
}

void ApplyFriend::resetLabels()
{
    const int HORIZONTAL_SPACING = 10;
    const int VERTICAL_SPACING = 10;
    const int START_X = 5;
    const int START_Y = 5;

    int currentX = START_X;
    int currentY = START_Y;
    int maxHeight = START_Y;

    for(auto & key : _friend_label_keys) {
        auto lb = _friend_labels[key];

        // 【关键】强制控件根据内容调整自己的实际大小（包含padding）
        lb->adjustSize();

        int labelWidth = lb->width();
        int labelHeight = lb->height();

        // 检查是否需要换行
        if (currentX + labelWidth > ui->gridWidget->width() - START_X) {
            currentX = START_X;
            currentY += labelHeight + VERTICAL_SPACING;
        }

        lb->move(currentX, currentY);
        lb->show();

        currentX += labelWidth + HORIZONTAL_SPACING;
        maxHeight = qMax(maxHeight, currentY + labelHeight);
    }

    // 动态调整父容器高度
    ui->gridWidget->setFixedHeight(maxHeight + VERTICAL_SPACING);
}

void ApplyFriend::addLabel(QString name)
{
    if(_friend_labels.size() >= 3){
        ADDMSG(ElaMessageBarType::Top,"标签已达上限!",this,0);
        return;
    }
    if (_friend_labels.find(name) != _friend_labels.end()) {
        return;
    }

    auto tmplabel = new FrienddLabel(ui->gridWidget);
    tmplabel->SetText(name);
    tmplabel->setObjectName("FriendLabel");

    // 连接删除信号
    connect(tmplabel, &FrienddLabel::sig_close, this, &ApplyFriend::SlotRemoveFriendLabel);

    // 加入数据结构
    _friend_labels[name] = tmplabel;
    _friend_label_keys.push_back(name);

    // 【关键】直接调用重排函数，不用自己算坐标了！
    resetLabels();

    ui->lb_ed->clear();
}

void ApplyFriend::ShowMoreLabel()
{
    ui->more_lb_wd->hide();

    ui->lb_list->setFixedWidth(325);

    // 使用统一的间距常量
    const int HORIZONTAL_SPACING = 10;  // 水平间距
    const int VERTICAL_SPACING = 10;    // 垂直间距
    const int START_X = 5;
    const int START_Y = 5;

    int currentX = START_X;
    int currentY = START_Y;
    int maxHeight = 0;

    // 重排现有的label
    for(auto & added_key : _add_label_keys){
        auto added_lb = _add_labels[added_key];

        // 获取标签的实际宽度
        int labelWidth = added_lb->width();
        int labelHeight = added_lb->height();

        // 检查是否需要换行
        if (currentX + labelWidth > ui->lb_list->width() - START_X) {
            currentX = START_X;
            currentY += labelHeight + VERTICAL_SPACING;
        }

        // 设置位置
        added_lb->move(currentX, currentY);

        // 更新下一个标签的X坐标
        currentX += labelWidth + HORIZONTAL_SPACING;
        maxHeight = qMax(maxHeight, currentY + labelHeight);
    }

    // 添加未添加的
    for(int i = 0; i < static_cast<int>(_tip_data.size()); i++){
        auto iter = _add_labels.find(_tip_data[i]);
        if(iter != _add_labels.end()){
            continue;
        }

        auto* lb = new ClickedLabel(ui->lb_list);
        lb->SetState("normal", "hover", "pressed", "selected_normal",
                     "selected_hover", "selected_pressed");
        lb->setObjectName("tipslb");
        lb->setText(_tip_data[i]);
        connect(lb, &ClickedLabel::clicked, this, &ApplyFriend::SlotChangeFriendLabelByTip);

        lb->show();

        // 获取标签的实际宽度
        int labelWidth = lb->width();
        int labelHeight = lb->height();

        // 检查是否需要换行
        if (currentX + labelWidth > ui->lb_list->width() - START_X) {
            currentX = START_X;
            currentY += labelHeight + VERTICAL_SPACING;
        }

        // 设置位置
        lb->move(currentX, currentY);

        // 更新下一个标签的X坐标
        currentX += labelWidth + HORIZONTAL_SPACING;
        maxHeight = qMax(maxHeight, currentY + labelHeight);

        _add_labels.insert(lb->text(), lb);
        _add_label_keys.push_back(lb->text());
    }

    // 计算新的高度（加上底部边距）
    int newHeight = maxHeight + VERTICAL_SPACING;
    int oldHeight = ui->lb_list->height();

    // 设置新的高度
    ui->lb_list->setFixedHeight(newHeight);

    // 计算需要增加的高度
    int diff_height = newHeight - oldHeight;
    if (diff_height > 0) {
        ui->scrollcontent->setFixedHeight(ui->scrollcontent->height() + diff_height);
    }
}

void ApplyFriend::SlotLabelEnter()
{
    if(ui->lb_ed->text().isEmpty()){
        return;
    }
    if(_friend_labels.size() >= 3){
        ADDMSG(ElaMessageBarType::Top,"标签已达上限!",this,0);
        ui->lb_ed->clear();
        ui->input_tip_wid->hide();
        return;
    }

    auto text = ui->lb_ed->text();
    addLabel(text);

    ui->input_tip_wid->hide();
    auto find_it = std::find(_tip_data.begin(), _tip_data.end(), text);

    if (find_it == _tip_data.end()) {
        _tip_data.push_back(text);
    }

    auto find_add = _add_labels.find(text);
    if (find_add != _add_labels.end()) {
        find_add.value()->SetCurState(ClickLbState::Selected);
        return;
    }

    // 创建新标签
    auto* lb = new ClickedLabel(ui->lb_list);
    lb->SetState("normal", "hover", "pressed", "selected_normal",
                 "selected_hover", "selected_pressed");
    lb->setObjectName("tipslb");
    lb->setText(text);
    connect(lb, &ClickedLabel::clicked, this, &ApplyFriend::SlotChangeFriendLabelByTip);

    lb->show();

    _add_labels.insert(text, lb);
    _add_label_keys.push_back(text);

    // 重新布局所有标签
    relayoutAllTips();

    lb->SetCurState(ClickLbState::Selected);
}

void ApplyFriend::relayoutAllTips()
{
    const int HORIZONTAL_SPACING = 10;
    const int VERTICAL_SPACING = 10;
    const int START_X = 5;
    const int START_Y = 5;

    int currentX = START_X;
    int currentY = START_Y;
    int maxHeight = START_Y;

    for(auto & key : _add_label_keys) {
        auto lb = _add_labels[key];

        lb->adjustSize(); // 获取真实宽高
        int labelWidth = lb->width();
        int labelHeight = lb->height();

        if (currentX + labelWidth > ui->lb_list->width() - START_X) {
            currentX = START_X;
            currentY += labelHeight + VERTICAL_SPACING;
        }

        lb->move(currentX, currentY);
        lb->show();

        currentX += labelWidth + HORIZONTAL_SPACING;
        maxHeight = qMax(maxHeight, currentY + labelHeight);
    }

    int newHeight = maxHeight + VERTICAL_SPACING;
    int oldHeight = ui->lb_list->height();
    ui->lb_list->setFixedHeight(newHeight);

    int diff_height = newHeight - oldHeight;
    if (diff_height != 0) { // 这里改成 != 0，高度变小了也能缩回去
        ui->scrollcontent->setFixedHeight(ui->scrollcontent->height() + diff_height);
    }
}

void ApplyFriend::SlotRemoveFriendLabel(QString name)
{

    _label_point.setX(2);
    _label_point.setY(6);

    auto find_iter = _friend_labels.find(name);

    if(find_iter == _friend_labels.end()){
        return;
    }

    auto find_key = _friend_label_keys.end();
    for(auto iter = _friend_label_keys.begin(); iter != _friend_label_keys.end();
         iter++){
        if(*iter == name){
            find_key = iter;
            break;
        }
    }

    if(find_key != _friend_label_keys.end()){
        _friend_label_keys.erase(find_key);
    }


    delete find_iter.value();

    _friend_labels.erase(find_iter);

    resetLabels();

    auto find_add = _add_labels.find(name);
    if(find_add == _add_labels.end()){
        return;
    }

    find_add.value()->ResetNormalState();
}

//点击标已有签添加或删除新联系人的标签
void ApplyFriend::SlotChangeFriendLabelByTip(QString lbtext, ClickLbState state)
{
    auto find_iter = _add_labels.find(lbtext);
    if(find_iter == _add_labels.end()){
        return;
    }

    auto lb = find_iter.value();

    if(state == ClickLbState::Selected){

        // 超过限制
        if(_friend_labels.size() >= 3){
            ADDMSG(ElaMessageBarType::Top,"标签已达上限!",this,0);
            lb->ResetNormalState();
            return;
        }

        addLabel(lbtext);
        return;
    }

    if(state == ClickLbState::Normal){
        SlotRemoveFriendLabel(lbtext);
        return;
    }
}
void ApplyFriend::SlotLabelTextChange(const QString& text)
{
    if (text.isEmpty()) {
        ui->tip_lb->setText("");
        ui->input_tip_wid->hide();
        return;
    }

    auto iter = std::find(_tip_data.begin(), _tip_data.end(), text);
    if (iter == _tip_data.end()) {
        auto new_text = add_prefix + text;
        ui->tip_lb->setText(new_text);
        ui->input_tip_wid->show();
        return;
    }
    ui->tip_lb->setText(text);
    ui->input_tip_wid->show();
}

void ApplyFriend::SlotLabelEditFinished()
{
    ui->input_tip_wid->hide();
}

void ApplyFriend::SlotAddFirendLabelByClickTip(QString text)
{
    int index = text.indexOf(add_prefix);
    if (index != -1) {
        text = text.mid(index + add_prefix.length());
    }

    if(_friend_labels.size() >= 3){
        ADDMSG(ElaMessageBarType::Top,"标签已达上限!",this,0);
        ui->lb_ed->clear();
        ui->input_tip_wid->hide();
        return;
    }

    // 1. 添加到上方已选区域
    addLabel(text);

    // 2. 处理下方待选区域
    auto find_it = std::find(_tip_data.begin(), _tip_data.end(), text);
    if (find_it == _tip_data.end()) {
        _tip_data.push_back(text);
    }

    auto find_add = _add_labels.find(text);
    if (find_add != _add_labels.end()) {
        find_add.value()->SetCurState(ClickLbState::Selected);
        return;
    }

    auto* lb = new ClickedLabel(ui->lb_list);
    lb->SetState("normal", "hover", "pressed", "selected_normal",
                 "selected_hover", "selected_pressed");
    lb->setObjectName("tipslb");
    lb->setText(text);
    connect(lb, &ClickedLabel::clicked, this, &ApplyFriend::SlotChangeFriendLabelByTip);

    _add_labels.insert(text, lb);
    _add_label_keys.push_back(text);

    relayoutAllTips();

    lb->SetCurState(ClickLbState::Selected);
}

void ApplyFriend::SlotApplyCancel()
{
    QJsonObject jsonObj;
    auto uid = UserMgr::GetInstance()->GetUid();
    jsonObj["uid"] = uid;
    auto name = ui->name_ed->text();
    if(name.isEmpty()){
        name = ui->name_ed->placeholderText();
    }

    jsonObj["applyname"] = name;
    auto bakname = ui->back_ed->text();
    if(bakname.isEmpty()){
        bakname = ui->back_ed->placeholderText();
    }
    jsonObj["bakname"] = bakname;
    // 验证语 = 选中的申请标签
    QStringList tips;
    for (auto it = _add_labels.begin(); it != _add_labels.end(); ++it) {
        tips << it.key();
    }
    jsonObj["desc"] = tips.join("、");
    jsonObj["touid"] = _si->_uid;

    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    //发送tcp请求给chat server
    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_ADD_FRIEND_REQ, jsonData);

    this->hide();
    deleteLater();
}

void ApplyFriend::SlotApplySure()
{
    if (_si && UserMgr::GetInstance()->CheckFriendById(_si->_uid)) {
        ADDMSG(ElaMessageBarType::Top, "对方已是你的好友，无需重复添加", this, 0, 3000);
        this->hide();
        deleteLater();
        return;
    }

    QJsonObject obj;
    auto uid = UserMgr::GetInstance()->GetUid();
    auto name = ui->name_ed->text();
    if(name.isEmpty()){
        name = ui->name_ed->placeholderText();
    }
    auto back = ui->back_ed->text();
    if(back.isEmpty()){
        back = ui->back_ed->placeholderText();
    }
    obj["uid"] = uid;
    obj["applyname"] = name;
    obj["touid"] = _si->_uid;
    obj["bakname"] = back;

    QJsonDocument doc(obj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_ADD_FRIEND_REQ,jsonData);
    this->hide();
    deleteLater();
}
