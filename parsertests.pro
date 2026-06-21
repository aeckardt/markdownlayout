QT += core gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -fmessage-length=0

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/serialization/htmlimporter.cpp \
    src/serialization/htmlstyle.cpp \
    src/serialization/htmlwriter.cpp \
    src/serialization/inlineformatresolver.cpp \
    src/serialization/markdownimporter.cpp \
    src/serialization/markdowninlineparser.cpp \
    src/serialization/markdownwriter.cpp \
    src/textformat/blocktypes.cpp \
    src/textformat/constdefs.cpp \
    src/parsertests_main.cpp

HEADERS += \
    src/textformat/blocktypes.h \
    src/serialization/htmlimporter.h \
    src/serialization/htmlstyle.h \
    src/serialization/htmlwriter.h \
    src/serialization/inlineformatresolver.h \
    src/serialization/markdownimporter.h \
    src/serialization/markdowninlineparser.h \
    src/serialization/markdownwriter.h \
    src/textformat/constdefs.h \
    src/textformat/constdefs_p.h

INCLUDEPATH += \
    src

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
