QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = FileSplitterGUI
TEMPLATE = app
CONFIG += c++11

DEFINES += _CRT_SECURE_NO_WARNINGS

SOURCES += \
    main.cpp \
    FileSplitterCore.cpp \
    SplitWorker.cpp \
    MainWindow.cpp

HEADERS += \
    FileSplitterCore.h \
    SplitWorker.h \
    MainWindow.h

# Output directory
CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/Output
} else {
    DESTDIR = $$PWD/Output
}
