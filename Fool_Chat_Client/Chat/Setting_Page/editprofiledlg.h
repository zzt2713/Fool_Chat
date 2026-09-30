#ifndef EDITPROFILEDLG_H
#define EDITPROFILEDLG_H

#include "ElaDialog.h"
#include "global.h"
#include <QJsonObject>
#include <QMap>
#include <functional>

/******************************************************************************
*
* @file       editprofiledlg.h
* @brief      编辑个人资料弹窗（昵称/性别/头像，uid 只读）
*
* @author     Fool
* @date       2026/09/25
*****************************************************************************/

class ElaPersonPicture;
class ElaText;
class ElaLineEdit;
class ElaComboBox;
class ElaPushButton;

class EditProfileDlg : public ElaDialog
{
    Q_OBJECT
public:
    explicit EditProfileDlg(QWidget* parent = nullptr);
    ~EditProfileDlg();

private slots:
    // 设置模块 HTTP 回包分发
    void slot_setting_mod_finish(ReqId id, QString res, ErrorCodes err);

private:
    void initHandlers();
    void saveProfile();
    // 头像候选选中态：蓝色微光标记（ElaPushButton 自绘不吃 stylesheet）
    void applyIconSelection(ElaPushButton* btn);

    ElaPersonPicture* _avatarPreview{nullptr}; // 头像预览
    ElaLineEdit* _nickEdit{nullptr};           // 昵称输入框
    ElaLineEdit* _descEdit{nullptr};           // 个性签名输入框
    ElaComboBox* _sexComboBox{nullptr};        // 性别下拉，索引即 sex 取值（0保密/1男/2女）
    QList<ElaPushButton*> _iconBtns;           // 头像候选按钮（与候选路径同序）
    QString _selectedIcon;                     // 当前选中的头像资源路径
    QMap<ReqId, std::function<void(QJsonObject)>> _handlers; // 回包处理表
};

#endif // EDITPROFILEDLG_H
