QT       += core gui network sql multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

CONFIG += resources_big
RESOURCES += src.qrc
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Chat/About_Page/c_about.cpp \
    Chat/Admin_Page/adminwid.cpp \
    Chat/Chat_Comp/ClickedOnceLabel.cpp \
    Chat/Chat_Comp/adduseritem.cpp \
    Chat/Chat_Comp/applyfriend.cpp \
    Chat/Chat_Comp/applyfrienditem.cpp \
    Chat/Chat_Comp/applyfriendlist.cpp \
    Chat/Chat_Comp/applyfriendpage.cpp \
    Chat/Chat_Comp/authenfriend.cpp \
    Chat/Chat_Comp/contactuserlist.cpp \
    Chat/Chat_Comp/conuseritem.cpp \
    Chat/Chat_Comp/findsuccessdlg.cpp \
    Chat/Chat_Comp/frienddlabel.cpp \
    Chat/Chat_Comp/friendinfopage.cpp \
    Chat/Chat_Comp/grouptipitem.cpp \
    Chat/Chat_Comp/searchlist.cpp \
    Chat/Chat_Comp/userdata.cpp \
    Chat/Chat_Page/chatdialog.cpp \
    Chat/Contact_Page/contactdialog.cpp \
    Chat/Dynamic_Page/dynamic_card.cpp \
    Chat/Dynamic_Page/dynamic_detail.cpp \
    Chat/Dynamic_Page/dynamic_page.cpp \
    Chat/Dynamic_Page/newstar.cpp \
    Chat/Editor_Page/editor_page.cpp \
    Chat/Music_Page/musicpage.cpp \
    Chat/Notice_Page/msg_notice.cpp \
    Chat/Notice_Page/noticeitem.cpp \
    Chat/Notice_Page/noticepage.cpp \
    Chat/Setting_Page/editprofiledlg.cpp \
    Chat/Setting_Page/f_setting.cpp \
    Chat/Chat_Comp/bubbleframe.cpp \
    Chat/Chat_Comp/c_searchedit.cpp \
    Chat/c_window.cpp \
    Chat/Chat_Comp/chattext.cpp \
    Chat/Chat_Comp/chatuseritem.cpp \
    Chat/Chat_Comp/chatuserlist.cpp \
    Chat/Chat_Comp/chatview.cpp \
    Chat/Chat_Comp/chatwid.cpp \
    Chat/Chat_Comp/clikedbtn.cpp \
    Chat/Chat_Comp/listitembase.cpp \
    Chat/Chat_Comp/loadingdlg.cpp \
    Chat/Chat_Comp/messagetextedit.cpp \
    Chat/Chat_Comp/page_base.cpp \
    Chat/Chat_Comp/picturebubble.cpp \
    Chat/Chat_Comp/status_label.cpp \
    Chat/Chat_Comp/chatitembase.cpp \
    Chat/Chat_Comp/textbubble.cpp \
    src/core/Logger.cpp \
    src/widgets/clickedlabel.cpp \
    src/core/dbmanager.cpp \
    src/widgets/floatingtip.cpp \
    src/core/global.cpp \
    src/core/aimgr.cpp \
    src/core/httpmgr.cpp \
    src/ui/login.cpp \
    src/main.cpp \
    src/ui/mainwindow.cpp \
    src/widgets/masonryflow.cpp \
    src/widgets/msgtip.cpp \
    src/widgets/notifypopup.cpp \
    src/core/ranimg.cpp \
    src/ui/regsterdialog.cpp \
    src/ui/resetdialog.cpp \
    src/core/tcpmgr.cpp \
    src/widgets/timerbtn.cpp \
    src/core/usermgr.cpp

HEADERS += \
    Chat/About_Page/c_about.h \
    Chat/Admin_Page/adminwid.h \
    Chat/Chat_Comp/ClickedOnceLabel.h \
    Chat/Chat_Comp/adduseritem.h \
    Chat/Chat_Comp/applyfriend.h \
    Chat/Chat_Comp/applyfrienditem.h \
    Chat/Chat_Comp/applyfriendlist.h \
    Chat/Chat_Comp/applyfriendpage.h \
    Chat/Chat_Comp/authenfriend.h \
    Chat/Chat_Comp/contactuserlist.h \
    Chat/Chat_Comp/conuseritem.h \
    Chat/Chat_Comp/findsuccessdlg.h \
    Chat/Chat_Comp/frienddlabel.h \
    Chat/Chat_Comp/friendinfopage.h \
    Chat/Chat_Comp/grouptipitem.h \
    Chat/Chat_Comp/searchlist.h \
    Chat/Chat_Comp/userdata.h \
    Chat/Chat_Page/chatdialog.h \
    Chat/Contact_Page/contactdialog.h \
    Chat/Dynamic_Page/dynamic_card.h \
    Chat/Dynamic_Page/dynamic_detail.h \
    Chat/Dynamic_Page/dynamic_page.h \
    Chat/Dynamic_Page/newstar.h \
    Chat/Editor_Page/editor_page.h \
    Chat/Music_Page/musicpage.h \
    Chat/Notice_Page/msg_notice.h \
    Chat/Notice_Page/noticeitem.h \
    Chat/Notice_Page/noticepage.h \
    Chat/Setting_Page/editprofiledlg.h \
    Chat/Setting_Page/f_setting.h \
    Chat/Chat_Comp/bubbleframe.h \
    Chat/Chat_Comp/c_searchedit.h \
    Chat/c_window.h \
    Chat/Chat_Comp/chattext.h \
    Chat/Chat_Comp/chatuseritem.h \
    Chat/Chat_Comp/chatuserlist.h \
    Chat/Chat_Comp/chatview.h \
    Chat/Chat_Comp/chatwid.h \
    Chat/Chat_Comp/clikedbtn.h \
    Chat/Chat_Comp/listitembase.h \
    Chat/Chat_Comp/loadingdlg.h \
    Chat/Chat_Comp/MessageTextEdit.h \
    Chat/Chat_Comp/page_base.h \
    Chat/Chat_Comp/picturebubble.h \
    Chat/Chat_Comp/pixmaputil.h \
    Chat/Chat_Comp/status_label.h \
    Chat/Chat_Comp/chatitembase.h \
    Chat/Chat_Comp/textbubble.h \
    src/core/F_singleton.h \
    src/core/Logger.h \
    src/widgets/clickedlabel.h \
    src/core/dbmanager.h \
    src/widgets/floatingtip.h \
    src/core/global.h \
    src/core/aimgr.h \
    src/core/httpmgr.h \
    src/ui/login.h \
    src/ui/mainwindow.h \
    src/widgets/masonryflow.h \
    src/widgets/msgtip.h \
    src/widgets/notifypopup.h \
    src/core/ranimg.h \
    src/ui/regsterdialog.h \
    src/ui/resetdialog.h \
    src/core/tcpmgr.h \
    src/widgets/timerbtn.h \
    src/core/usermgr.h

FORMS += \
    Chat/About_Page/c_about.ui \
    Chat/Admin_Page/adminwid.ui \
    Chat/Chat_Comp/adduseritem.ui \
    Chat/Chat_Comp/applyfriend.ui \
    Chat/Chat_Comp/applyfrienditem.ui \
    Chat/Chat_Comp/applyfriendpage.ui \
    Chat/Chat_Comp/authenfriend.ui \
    Chat/Chat_Comp/conuseritem.ui \
    Chat/Chat_Comp/findsuccessdlg.ui \
    Chat/Chat_Comp/frienddlabel.ui \
    Chat/Chat_Comp/friendinfopage.ui \
    Chat/Chat_Comp/grouptipitem.ui \
    Chat/Chat_Page/chatdialog.ui \
    Chat/Contact_Page/contactdialog.ui \
    Chat/Dynamic_Page/dynamic_page.ui \
    Chat/Dynamic_Page/newstar.ui \
    Chat/Notice_Page/msg_notice.ui \
    Chat/Notice_Page/noticeitem.ui \
    Chat/Chat_Comp/chatuseritem.ui \
    Chat/Chat_Comp/chatwid.ui \
    Chat/Chat_Comp/loadingdlg.ui \
    src/ui/login.ui \
    src/ui/mainwindow.ui \
    src/ui/regsterdialog.ui \
    src/ui/resetdialog.ui

UI_DIR = ./generated
RCC_DIR = ./generated
MOC_DIR = ./generated
DESTDIR = ./bin

win32: system(mkdir $$shell_path($$OUT_PWD/generated) 2>nul || cd .)

RC_ICONS = logo/head.ico

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


DISTFILES += \
    config.ini

INCLUDEPATH += $$PWD/src $$PWD/src/core $$PWD/src/widgets $$PWD/Chat $$PWD/Chat/Chat_Comp

# ElaWidgetTools 库配置
ELAWIDGETTOOLS_ROOT = D:/cppsoft/RabbitEla
INCLUDEPATH += $$ELAWIDGETTOOLS_ROOT/include
LIBS += -L$$ELAWIDGETTOOLS_ROOT/lib -lElaWidgetTools

CONFIG(debug, debug | release) {
    DLL_SRC = D:/cppsoft/RabbitEla/bin/ElaWidgetTools.dll
    DLL_SRC = $$replace(DLL_SRC, /, \\)
    DLL_DST = $$OUT_PWD/$$DESTDIR/ElaWidgetTools.dll
    DLL_DST = $$replace(DLL_DST, /, \\)
    QMAKE_POST_LINK += copy /Y \"$$DLL_SRC\" \"$$DLL_DST\" &

    TargetConfig = $${PWD}/config.ini
    TargetConfig = $$replace(TargetConfig, /, \\)
    OutputDir =  $${OUT_PWD}/$${DESTDIR}
    OutputDir = $$replace(OutputDir, /, \\)
    //执行copy命令
    QMAKE_POST_LINK += copy /Y \"$$TargetConfig\" \"$$OutputDir\" &

    # 首先，定义static文件夹的路径
    StaticDir = $${PWD}/static
    # 将路径中的"/"替换为"\"
    StaticDir = $$replace(StaticDir, /, \\)
    #message($${StaticDir})
    QMAKE_POST_LINK += xcopy /Y /E /I \"$$StaticDir\" \"$$OutputDir\\static\\\"

}else{
    #release
    message("release mode")

    DLL_SRC = D:/cppsoft/RabbitEla/bin/ElaWidgetTools.dll
    DLL_SRC = $$replace(DLL_SRC, /, \\)
    DLL_DST = $$OUT_PWD/$$DESTDIR/ElaWidgetTools.dll
    DLL_DST = $$replace(DLL_DST, /, \\)
    QMAKE_POST_LINK += copy /Y \"$$DLL_SRC\" \"$$DLL_DST\" &

    TargetConfig = $${PWD}/config.ini
    #将输入目录中的"/"替换为"\"
    TargetConfig = $$replace(TargetConfig, /, \\)
    #将输出目录中的"/"替换为"\"
    OutputDir =  $${OUT_PWD}/$${DESTDIR}
    OutputDir = $$replace(OutputDir, /, \\)
    //执行copy命令
    QMAKE_POST_LINK += copy /Y \"$$TargetConfig\" \"$$OutputDir\"

    # 首先，定义static文件夹的路径
    StaticDir = $${PWD}/static
    # 将路径中的"/"替换为"\"
    StaticDir = $$replace(StaticDir, /, \\)
    #message($${StaticDir})
    # 使用xcopy命令拷贝文件夹，/E表示拷贝子目录及其内容，包括空目录。/I表示如果目标不存在则创建目录。/Y表示覆盖现有文件而不提示。
     QMAKE_POST_LINK += xcopy /Y /E /I \"$$StaticDir\" \"$$OutputDir\\static\\\"
}


win32-msvc*:QMAKE_CXXFLAGS += /wd"4819" /utf-8
