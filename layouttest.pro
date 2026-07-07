QT += core gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -fmessage-length=0

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/3rdparty/md4c/md4c.c \
    src/dialogs/linkeditordialog.cpp \
    src/documentlayout/markdownlayout.cpp \
    src/io/htmlwriter.cpp \
    src/io/inlineformatresolver.cpp \
    src/io/htmlimporter.cpp \
    src/io/htmlstyle.cpp \
    src/io/markdownhtmlparser.cpp \
    src/io/markdownimporter.cpp \
    src/io/markdownwriter.cpp \
    src/texteditor/texteditor.cpp \
    src/textformat/blocktypes.cpp \
    src/texteditor/texteditorwidget.cpp \
    src/textformat/constdefs.cpp \
    src/widgets/gradientbutton.cpp \
    src/widgets/toolbarseparator.cpp \
    src/layouttest_main.cpp

HEADERS += \
    src/3rdparty/md4c/md4c.h \
    src/dialogs/linkeditordialog.h \
    src/documentlayout/markdownlayout.h \
    src/io/htmlwriter.h \
    src/io/inlineformatresolver.h \
    src/io/htmlimporter.h \
    src/io/htmlstyle.h \
    src/io/markdownhtmlparser.h \
    src/io/markdownimporter.h \
    src/io/markdownwriter.h \
    src/texteditor/texteditor.h \
    src/texteditor/texteditorwidget.h \
    src/textformat/blocktypes.h \
    src/textformat/constdefs.h \
    src/widgets/gradientbutton.h \
    src/widgets/toolbarseparator.h

INCLUDEPATH += \
    src

OTHER_FILES += \
    README.md \
    THIRD_PARTY_NOTICES.md \
    .gitignore

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
