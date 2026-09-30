cmake_minimum_required(VERSION 3.16)

project(Chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# AUTOMOC/AUTOUIC/AUTORCC 的产物对应 .pro 里的 MOC_DIR/UI_DIR/RCC_DIR(./generated)，统一落在 build 目录
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# .pro: DESTDIR = ./bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Sql
    Multimedia
    MultimediaWidgets
)

set(SOURCES
    Chat/About_Page/c_about.cpp
    Chat/Admin_Page/adminwid.cpp
    Chat/Chat_Comp/ClickedOnceLabel.cpp
    Chat/Chat_Comp/adduseritem.cpp
    Chat/Chat_Comp/applyfriend.cpp
    Chat/Chat_Comp/applyfrienditem.cpp
    Chat/Chat_Comp/applyfriendlist.cpp
    Chat/Chat_Comp/applyfriendpage.cpp
    Chat/Chat_Comp/authenfriend.cpp
    Chat/Chat_Comp/contactuserlist.cpp
    Chat/Chat_Comp/conuseritem.cpp
    Chat/Chat_Comp/findsuccessdlg.cpp
    Chat/Chat_Comp/frienddlabel.cpp
    Chat/Chat_Comp/friendinfopage.cpp
    Chat/Chat_Comp/grouptipitem.cpp
    Chat/Chat_Comp/searchlist.cpp
    Chat/Chat_Comp/userdata.cpp
    Chat/Chat_Page/chatdialog.cpp
    Chat/Contact_Page/contactdialog.cpp
    Chat/Dynamic_Page/dynamic_card.cpp
    Chat/Dynamic_Page/dynamic_detail.cpp
    Chat/Dynamic_Page/dynamic_page.cpp
    Chat/Dynamic_Page/newstar.cpp
    Chat/Editor_Page/editor_page.cpp
    Chat/Music_Page/musicpage.cpp
    Chat/Notice_Page/msg_notice.cpp
    Chat/Notice_Page/noticeitem.cpp
    Chat/Notice_Page/noticepage.cpp
    Chat/Setting_Page/editprofiledlg.cpp
    Chat/Setting_Page/f_setting.cpp
    Chat/Chat_Comp/bubbleframe.cpp
    Chat/Chat_Comp/c_searchedit.cpp
    Chat/c_window.cpp
    Chat/Chat_Comp/chattext.cpp
    Chat/Chat_Comp/chatuseritem.cpp
    Chat/Chat_Comp/chatuserlist.cpp
    Chat/Chat_Comp/chatview.cpp
    Chat/Chat_Comp/chatwid.cpp
    Chat/Chat_Comp/clikedbtn.cpp
    Chat/Chat_Comp/listitembase.cpp
    Chat/Chat_Comp/loadingdlg.cpp
    Chat/Chat_Comp/messagetextedit.cpp
    Chat/Chat_Comp/page_base.cpp
    Chat/Chat_Comp/picturebubble.cpp
    Chat/Chat_Comp/status_label.cpp
    Chat/Chat_Comp/chatitembase.cpp
    Chat/Chat_Comp/textbubble.cpp
    src/core/Logger.cpp
    src/widgets/clickedlabel.cpp
    src/core/dbmanager.cpp
    src/widgets/floatingtip.cpp
    src/core/global.cpp
    src/core/aimgr.cpp
    src/core/httpmgr.cpp
    src/ui/login.cpp
    src/main.cpp
    src/ui/mainwindow.cpp
    src/widgets/masonryflow.cpp
    src/widgets/msgtip.cpp
    src/widgets/notifypopup.cpp
    src/core/ranimg.cpp
    src/ui/regsterdialog.cpp
    src/ui/resetdialog.cpp
    src/core/tcpmgr.cpp
    src/widgets/timerbtn.cpp
    src/core/usermgr.cpp
)

set(HEADERS
    Chat/About_Page/c_about.h
    Chat/Admin_Page/adminwid.h
    Chat/Chat_Comp/ClickedOnceLabel.h
    Chat/Chat_Comp/adduseritem.h
    Chat/Chat_Comp/applyfriend.h
    Chat/Chat_Comp/applyfrienditem.h
    Chat/Chat_Comp/applyfriendlist.h
    Chat/Chat_Comp/applyfriendpage.h
    Chat/Chat_Comp/authenfriend.h
    Chat/Chat_Comp/contactuserlist.h
    Chat/Chat_Comp/conuseritem.h
    Chat/Chat_Comp/findsuccessdlg.h
    Chat/Chat_Comp/frienddlabel.h
    Chat/Chat_Comp/friendinfopage.h
    Chat/Chat_Comp/grouptipitem.h
    Chat/Chat_Comp/searchlist.h
    Chat/Chat_Comp/userdata.h
    Chat/Chat_Page/chatdialog.h
    Chat/Contact_Page/contactdialog.h
    Chat/Dynamic_Page/dynamic_card.h
    Chat/Dynamic_Page/dynamic_detail.h
    Chat/Dynamic_Page/dynamic_page.h
    Chat/Dynamic_Page/newstar.h
    Chat/Editor_Page/editor_page.h
    Chat/Music_Page/musicpage.h
    Chat/Notice_Page/msg_notice.h
    Chat/Notice_Page/noticeitem.h
    Chat/Notice_Page/noticepage.h
    Chat/Setting_Page/editprofiledlg.h
    Chat/Setting_Page/f_setting.h
    Chat/Chat_Comp/bubbleframe.h
    Chat/Chat_Comp/c_searchedit.h
    Chat/c_window.h
    Chat/Chat_Comp/chattext.h
    Chat/Chat_Comp/chatuseritem.h
    Chat/Chat_Comp/chatuserlist.h
    Chat/Chat_Comp/chatview.h
    Chat/Chat_Comp/chatwid.h
    Chat/Chat_Comp/clikedbtn.h
    Chat/Chat_Comp/listitembase.h
    Chat/Chat_Comp/loadingdlg.h
    Chat/Chat_Comp/MessageTextEdit.h
    Chat/Chat_Comp/page_base.h
    Chat/Chat_Comp/picturebubble.h
    Chat/Chat_Comp/pixmaputil.h
    Chat/Chat_Comp/status_label.h
    Chat/Chat_Comp/chatitembase.h
    Chat/Chat_Comp/textbubble.h
    src/core/F_singleton.h
    src/core/Logger.h
    src/widgets/clickedlabel.h
    src/core/dbmanager.h
    src/widgets/floatingtip.h
    src/core/global.h
    src/core/aimgr.h
    src/core/httpmgr.h
    src/ui/login.h
    src/ui/mainwindow.h
    src/widgets/masonryflow.h
    src/widgets/msgtip.h
    src/widgets/notifypopup.h
    src/core/ranimg.h
    src/ui/regsterdialog.h
    src/ui/resetdialog.h
    src/core/tcpmgr.h
    src/widgets/timerbtn.h
    src/core/usermgr.h
)

set(FORMS
    Chat/About_Page/c_about.ui
    Chat/Admin_Page/adminwid.ui
    Chat/Chat_Comp/adduseritem.ui
    Chat/Chat_Comp/applyfriend.ui
    Chat/Chat_Comp/applyfrienditem.ui
    Chat/Chat_Comp/applyfriendpage.ui
    Chat/Chat_Comp/authenfriend.ui
    Chat/Chat_Comp/conuseritem.ui
    Chat/Chat_Comp/findsuccessdlg.ui
    Chat/Chat_Comp/frienddlabel.ui
    Chat/Chat_Comp/friendinfopage.ui
    Chat/Chat_Comp/grouptipitem.ui
    Chat/Chat_Page/chatdialog.ui
    Chat/Contact_Page/contactdialog.ui
    Chat/Dynamic_Page/dynamic_page.ui
    Chat/Dynamic_Page/newstar.ui
    Chat/Notice_Page/msg_notice.ui
    Chat/Notice_Page/noticeitem.ui
    Chat/Chat_Comp/chatuseritem.ui
    Chat/Chat_Comp/chatwid.ui
    Chat/Chat_Comp/loadingdlg.ui
    src/ui/login.ui
    src/ui/mainwindow.ui
    src/ui/regsterdialog.ui
    src/ui/resetdialog.ui
)

# 以下资源文件在部分副本目录中不存在，缺失时跳过，避免 configure 直接失败

# .pro: RC_ICONS = logo/head.ico
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/logo/head.ico")
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    file(WRITE "${APP_ICON_RC}" "IDI_ICON1 ICON \"${CMAKE_CURRENT_SOURCE_DIR}/logo/head.ico\"\n")
    list(APPEND SOURCES ${APP_ICON_RC})
endif()

# .pro: RESOURCES += src.qrc，Qt6 AUTORCC 已覆盖 resources_big
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src.qrc")
    list(APPEND SOURCES src.qrc)
endif()

add_executable(Chat WIN32
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
)

target_include_directories(Chat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/Chat
    ${CMAKE_CURRENT_SOURCE_DIR}/Chat/Chat_Comp
)

# ElaWidgetTools 库配置
set(ELAWIDGETTOOLS_ROOT "D:/cppsoft/RabbitEla")
target_include_directories(Chat PRIVATE ${ELAWIDGETTOOLS_ROOT}/include)
target_link_directories(Chat PRIVATE ${ELAWIDGETTOOLS_ROOT}/lib)

target_link_libraries(Chat PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    ElaWidgetTools
)

if(MSVC)
    target_compile_options(Chat PRIVATE /wd4819 /utf-8)
endif()

set(POST_BUILD_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ELAWIDGETTOOLS_ROOT}/bin/ElaWidgetTools.dll"
            "$<TARGET_FILE_DIR:Chat>"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
    list(APPEND POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
                "$<TARGET_FILE_DIR:Chat>"
    )
endif()

# .pro 用 xcopy /E /I 拷贝 static 目录
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/static")
    list(APPEND POST_BUILD_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/static"
                "$<TARGET_FILE_DIR:Chat>/static"
    )
endif()

add_custom_command(TARGET Chat POST_BUILD ${POST_BUILD_COMMANDS})
