#include "textbubble.h"
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QtMath>
#include "ElaTheme.h"

TextBubble::TextBubble(ChatRole role, const QString &text, QWidget *parent)
    :BubbleFrame(role, parent)
{
    m_pTextEdit = new QTextEdit();
    m_pTextEdit->setReadOnly(true);
    m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pTextEdit->installEventFilter(this);
    QFont font("Microsoft YaHei");
    font.setPointSize(12);
    m_pTextEdit->setFont(font);
    setPlainText(text);
    setWidget(m_pTextEdit);
    // 文字颜色绑定主题：带样式表的 QTextEdit 不吃应用 palette 更新，
    // 不绑定的话颜色会冻结在创建时的主题（亮暗切换后字色不跟着变）
    auto applyTextColor = [this](ElaThemeType::ThemeMode mode) {
        const QColor color = eTheme->getThemeColor(mode, ElaThemeType::BasicText);
        m_pTextEdit->setStyleSheet(
            QStringLiteral("QTextEdit{background:transparent;border:none;color:%1;}").arg(color.name()));
        this->update(); // 气泡底色也在主题里（BubbleFrame 自绘），一并重绘
    };
    applyTextColor(eTheme->getThemeMode());
    connect(eTheme, &ElaTheme::themeModeChanged, m_pTextEdit, applyTextColor);
}

bool TextBubble::eventFilter(QObject *o, QEvent *e)
{
    if(m_pTextEdit == o && e->type() == QEvent::Paint)
    {
        adjustTextHeight(); //PaintEvent中设置
    }
    return BubbleFrame::eventFilter(o, e);
}

void TextBubble::adjustTextHeight()
{
    QTextDocument *doc = m_pTextEdit->document();
    // updateMaxWidth 会把排版宽度置成自然宽，这里恢复成 QTextEdit 同款的视口宽度再量
    const int vw = m_pTextEdit->viewport()->width();
    if (vw > 0) {
        doc->setPageSize(QSize(vw, -1));
    }
    // 文档整体尺寸与滚动条判定同源（含段落间距和文档边距）：气泡取它，不矮（可滚）也不胖（留白）
    const int contentHeight = qCeil(doc->documentLayout()->documentSize().height());
    int vMargin = this->layout()->contentsMargins().top();
    setFixedHeight(contentHeight + vMargin * 2);
}

void TextBubble::setPlainText(const QString &text)
{
    m_pTextEdit->setPlainText(text);
    updateMaxWidth();
}

void TextBubble::setMarkdownText(const QString& text)
{
    m_pTextEdit->document()->setMarkdown(text);
    updateMaxWidth();
    adjustTextHeight();
}

void TextBubble::updateMaxWidth()
{
    qreal doc_margin = m_pTextEdit->document()->documentMargin();
    int margin_left = this->layout()->contentsMargins().left();
    int margin_right = this->layout()->contentsMargins().right();

    // 获取整个文档的理想宽度
    m_pTextEdit->document()->setTextWidth(-1); // 先取消固定宽度
    QSizeF doc_size = m_pTextEdit->document()->size();
    int max_width = doc_size.width();

    // 设置这个气泡的最大宽度
    setMaximumWidth(max_width + doc_margin * 2 + (margin_left + margin_right));
}

