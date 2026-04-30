/**
 * @file main.cpp
 * @brief FileSplitter GUI 应用程序入口
 *
 * 创建 QApplication 和主窗口，启动 Qt 事件循环。
 * 项目基于 Qt 5.6，启用 C++11 特性。
 */

#include "MainWindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序元信息，用于 QSettings 等组件的组织标识
    QCoreApplication::setApplicationName("FileSplitter");
    QCoreApplication::setApplicationVersion("2.0.0");

    // 创建并显示主窗口
    MainWindow w;
    w.show();

    // 进入 Qt 事件循环，直到窗口关闭
    return a.exec();
}
