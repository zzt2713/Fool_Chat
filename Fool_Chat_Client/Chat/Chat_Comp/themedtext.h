#ifndef THEMEDTEXT_H
#define THEMEDTEXT_H

#include "ElaText.h"
#include "ElaTheme.h"

// ElaText 自带样式表后不吃应用 palette 更新，颜色可能停在创建时的主题：
// 用 QSS color（优先级高于 palette）绑定主题切换，强制对齐 BasicText
inline void BindTextToTheme(ElaText* text)
{
    auto apply = [text](ElaThemeType::ThemeMode mode) {
        const QColor color = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
        text->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
    };
    apply(eTheme->getThemeMode());
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, text, apply);
}

// 次级文字（最后消息预览/申请描述/分组名）：不参与纯黑白切换，
// 亮色淡灰、暗色淡白
inline void BindMutedTextToTheme(QWidget* text)
{
    auto apply = [text](ElaThemeType::ThemeMode mode) {
        const QColor color = (mode == ElaThemeType::Dark) ? QColor("#B5B5B5")
                                                          : QColor("#909399");
        text->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
    };
    apply(eTheme->getThemeMode());
    QObject::connect(eTheme, &ElaTheme::themeModeChanged, text, apply);
}

#endif // THEMEDTEXT_H
