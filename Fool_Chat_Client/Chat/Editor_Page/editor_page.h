#ifndef EDITOR_PAGE_H
#define EDITOR_PAGE_H
#include "../Chat_Comp/page_base.h"
#include "ElaMarkdownViewer.h"

class ElaCodeEditor;
// editor_page.h
class EditorPage : public Page_Base {
    Q_OBJECT
public:
    explicit EditorPage(QWidget *parent = nullptr);
    ~EditorPage() override;

private:
    ElaCodeEditor *_codeEditor;
    ElaMarkdownViewer *_set_Md;
    QTimer *_updateTimer;
    QString _lastCode;
    int _index;
};


#endif // EDITOR_PAGE_H
