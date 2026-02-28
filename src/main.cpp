#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) // C++ 标准入口函数
{
    QApplication a(argc, argv); // 创建应用对象，负责管理应用级资源和主事件循环

    MainWindow w; // 创建主窗口
    w.show();     // 显示主窗口到屏幕上

    return a.exec(); // 进入事件循环，程序在这里等待用户操作
                     // 只有当用户关闭窗口时，这个函数才会返回
}
