#ifndef F_SETTING_H
#define F_SETTING_H

#include "ranimg.h"
#include "ElaScrollPage.h"
#include "ElaWindow.h"
#include <QVariant>
/******************************************************************************
*
* @file       f_setting.h
* @brief      设置页 Function
*
* @author     Fool
* @date       2026/03/03
* @history
*****************************************************************************/

class ElaRadioButton;
class ElaToggleSwitch;
class ElaComboBox;
class ElaToolButton;
class F_Setting : public ElaScrollPage
{
public:
    F_Setting(QWidget* parent);
    ~F_Setting();

private:
    // 界面偏好持久化到 ui_settings.ini（主题/窗口效果/导航/切换效果/用户卡片等）
    void saveUiSettings(const QString& key, const QVariant& value);

private:
    ElaComboBox* _themeComboBox{nullptr};
    ElaComboBox* _otherComboBox{nullptr};
    ElaRadioButton* _normalButton{nullptr};

    ElaRadioButton* _windowNormalButton{nullptr};
    ElaRadioButton* _windowPixmapButton{nullptr};
    ElaRadioButton* _windowMovieButton{nullptr};

    ElaRadioButton* _elaMicaButton{nullptr};
#ifdef Q_OS_WIN
    ElaRadioButton* _micaButton{nullptr};
    ElaRadioButton* _micaAltButton{nullptr};
    ElaRadioButton* _acrylicButton{nullptr};
    ElaRadioButton* _dwmBlurnormalButton{nullptr};
#endif
    ElaToggleSwitch* _userCardSwitchButton{nullptr};
    ElaRadioButton* _minimumButton{nullptr};
    ElaRadioButton* _compactButton{nullptr};
    ElaRadioButton* _maximumButton{nullptr};
    ElaRadioButton* _autoButton{nullptr};

    ElaRadioButton* _noneButton{nullptr};
    ElaRadioButton* _popupButton{nullptr};
    ElaRadioButton* _scaleButton{nullptr};
    ElaRadioButton* _flipButton{nullptr};
    ElaRadioButton* _blurButton{nullptr};

};

#endif // F_SETTING_H
