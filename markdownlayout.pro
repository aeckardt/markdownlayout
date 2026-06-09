QT += core gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        qtextdocumentlayout/qcssutil.cpp \
        qtextdocumentlayout/qtextdocumentlayout.cpp

HEADERS += \
        mainwindow.h \
        qtextdocumentlayout/qcssparser_p.h \
        qtextdocumentlayout/qcssutil_p.h \
        qtextdocumentlayout/qfixed_p.h \
        qtextdocumentlayout/qfont_p.h \
        qtextdocumentlayout/qtextdocumentlayout.h \
        qtextdocumentlayout/qtextimagehandler_p.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
