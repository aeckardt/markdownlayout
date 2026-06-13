QT += core gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -fmessage-length=0

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/texteditor/inlineformatresolver.cpp \
    src/texteditor/htmlexporter.cpp \
    src/texteditor/htmlimporter.cpp \
    src/texteditor/htmlstyle.cpp \
    src/texteditor/markdownexporter.cpp \
    src/texteditor/markdownimporter.cpp \
    src/texteditor/markdowninlineparser.cpp \
    src/texteditor/texteditorstyle.cpp \
    src/tests.cpp

HEADERS += \
    src/texteditor/inlineformatresolver.h \
    src/texteditor/htmlexporter.h \
    src/texteditor/htmlimporter.h \
    src/texteditor/htmlstyle.h \
    src/texteditor/markdownexporter.h \
    src/texteditor/markdownimporter.h \
    src/texteditor/markdowninlineparser.h \
    src/texteditor/texteditorstyle.h

INCLUDEPATH += \
    src \
    src/texteditor

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
