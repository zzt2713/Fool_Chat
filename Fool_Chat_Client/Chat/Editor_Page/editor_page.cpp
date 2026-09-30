#include "editor_page.h"
#include "ElaCodeEditor.h"
#include "ElaComboBox.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaPushButton.h"
#include "ElaIcon.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include "ElaTheme.h"
#include <QEvent>
#include <QTimer>
#include "editor_page.h"
#include "ElaCodeEditor.h"
#include "ElaComboBox.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaPushButton.h"
#include "ElaIcon.h"
#include "ElaMarkdownViewer.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include "qsplitter.h"

static QString getCppDemo()
{
    return "// demo...";
}

EditorPage::EditorPage(QWidget *parent)
    : Page_Base(parent), _index(0)
{
    setWindowTitle("代码编辑器");
    createCustomWidget("支持行号显示与多语言语法高亮\n");

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("代码编辑器");
    ElaComboBox *langCombo = new ElaComboBox(this);
    langCombo->addItems({"C++", "C", "C#", "Python", "JavaScript", "Lua", "Rust", "PHP", "Markdown"});
    langCombo->setCurrentIndex(0);

    ElaPushButton *insertFileBtn = new ElaPushButton("插入文件", this);
    insertFileBtn->setElaIcon(ElaIconType::FileImport);
    insertFileBtn->setFixedWidth(150);

    ElaPushButton *saveFileBtn = new ElaPushButton("保存文件", this);
    saveFileBtn->setElaIcon(ElaIconType::FileArrowDown);
    saveFileBtn->setFixedWidth(150);

    ElaScrollPageArea *langArea = new ElaScrollPageArea(this);
    QHBoxLayout *langLayout = new QHBoxLayout(langArea);
    langLayout->addWidget(new ElaText("语言", 15, this));
    langLayout->addWidget(langCombo);
    langLayout->addSpacing(5);
    langLayout->addWidget(insertFileBtn);
    langLayout->addSpacing(5);
    langLayout->addWidget(saveFileBtn);
    langLayout->addStretch();

    _codeEditor = new ElaCodeEditor(this);
    _codeEditor->setIsReadOnly(false);
    _codeEditor->setCode(getCppDemo());
    _lastCode = getCppDemo();

    _set_Md = new ElaMarkdownViewer(this);
    _set_Md->setBorderRadius(8);
    _set_Md->setMarkdown(getCppDemo());
    _set_Md->setVisible(_index == 8);

    _updateTimer = new QTimer(this);
    _updateTimer->setInterval(300);

    connect(_updateTimer, &QTimer::timeout, this, [=]() {
        if (_index != 8) {
            return;
        }

        QString currentCode = _codeEditor->getCode();

        if (currentCode != _lastCode) {
            _lastCode = currentCode;
            _set_Md->setMarkdown(currentCode);
        }
    });

    _updateTimer->start();

    // 语言切换
    connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        _index = index;

        if (index < 8) {  // 代码模式
            _codeEditor->setLanguage(static_cast<ElaCodeEditor::Language>(index));
            _codeEditor->setCode(_codeEditor->getCode());
            _lastCode = _codeEditor->getCode();
            _set_Md->setVisible(false);  // 隐藏预览
        } else {  // Markdown 模式
            _codeEditor->setLanguage(ElaCodeEditor::Language::CPP);
            _codeEditor->setCode(_codeEditor->getCode());
            _lastCode =_codeEditor->getCode();
            _set_Md->setVisible(true);  // 显示预览
            _set_Md->setMarkdown(_codeEditor->getCode());
        }
    });

    connect(insertFileBtn, &ElaPushButton::clicked, this, [=]() {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            "选择文件",
            "",
            "文本文件 (*.txt *.cpp *.h *.py *.js *.php *.md);;所有文件 (*.*)"
            );
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString content = QString::fromUtf8(file.readAll());
                _codeEditor->setCode(content);
                _lastCode = content;

                if (_index == 8) {
                    _set_Md->setMarkdown(content);
                }
                file.close();
            }
        }
    });

    // 保存文件
    connect(saveFileBtn, &ElaPushButton::clicked, this, [=]() {
        QString saveFile("");
        QString extension("");
        switch(_index) {
        case 0: saveFile = "C++ 文件(*.cpp)"; extension = "未命名.cpp"; break;
        case 1: saveFile = "C 文件 (*.c)"; extension = "未命名.c"; break;
        case 2: saveFile = "C# 文件 (*.cs)"; extension = "未命名.cs"; break;
        case 3: saveFile = "Python 文件 (*.py)"; extension = "未命名.py"; break;
        case 4: saveFile = "JavaScript 文件 (*.js)"; extension = "未命名.js"; break;
        case 5: saveFile = "Lua 文件 (*.lua)"; extension = "未命名.lua"; break;
        case 6: saveFile = "Rust 文件 (*.rs)"; extension = "未命名.rs"; break;
        case 7: saveFile = "PHP 文件 (*.php)"; extension = "未命名.php"; break;
        case 8: saveFile = "Markdown 文件 (*.md)"; extension = "未命名.md"; break;
        }

        QString fileName = QFileDialog::getSaveFileName(
            this,
            "保存文件",
            extension,
            saveFile + ";; 文本文件 (*.txt);;所有文件 (*.*)"
            );
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(_codeEditor->getCode().toUtf8());
                file.close();
            }
        }
    });

    // 左右分屏
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(_codeEditor);
    splitter->addWidget(_set_Md);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    QVBoxLayout *centerVLayout = new QVBoxLayout(centralWidget);
    centerVLayout->setContentsMargins(0, 0, 0, 0);
    centerVLayout->addWidget(langArea);
    centerVLayout->addWidget(splitter);

    addCentralWidget(centralWidget, true, false, 0);

    // 主题切换
    connect(eTheme, &ElaTheme::themeModeChanged, langCombo, [=]() {
        if (eTheme->getThemeMode() == ElaThemeType::Dark) {
            langCombo->setStyleSheet("QComboBox { color: white; } QComboBox QAbstractItemView { color: white; background-color: #1E1E1E; }");
        } else {
            langCombo->setStyleSheet("QComboBox { color: black; } QComboBox QAbstractItemView { color: black; }");
        }
        langCombo->update();
    });
}

EditorPage::~EditorPage()
{
}
