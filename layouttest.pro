QT += core gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -fmessage-length=0

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/layouttest_main.cpp \
    src/texteditor/blocktypes.cpp \
    src/texteditor/gradientbutton.cpp \
    src/texteditor/inlineformatresolver.cpp \
    src/texteditor/htmlexporter.cpp \
    src/texteditor/htmlimporter.cpp \
    src/texteditor/htmlstyle.cpp \
    src/texteditor/linkeditordialog.cpp \
    src/texteditor/markdownexporter.cpp \
    src/texteditor/markdownimporter.cpp \
    src/texteditor/markdowninlineparser.cpp \
    src/texteditor/markdownlayout.cpp \
    src/texteditor/texteditor.cpp \
    src/texteditor/texteditorstyle.cpp \
    src/texteditor/texteditorwidget.cpp \
    src/texteditor/toolbarseparator.cpp

HEADERS += \
    src/texteditor/blocktypes.h \
    src/texteditor/gradientbutton.h \
    src/texteditor/htmlimporter_p.h \
    src/texteditor/inlineformatresolver.h \
    src/texteditor/htmlexporter.h \
    src/texteditor/htmlimporter.h \
    src/texteditor/htmlstyle.h \
    src/texteditor/linkeditordialog.h \
    src/texteditor/markdownexporter.h \
    src/texteditor/markdownimporter.h \
    src/texteditor/markdownimporter_p.h \
    src/texteditor/markdowninlineparser.h \
    src/texteditor/markdownlayout.h \
    src/texteditor/texteditor.h \
    src/texteditor/texteditor_p.h \
    src/texteditor/texteditorstyle.h \
    src/texteditor/texteditorstyle_p.h \
    src/texteditor/texteditorwidget.h \
    src/texteditor/toolbarseparator.h

INCLUDEPATH += \
    src \
    src/texteditor

OTHER_FILES += \
    README.md \
    THIRD_PARTY_NOTICES.md

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
