#include <QApplication>
#include "SerialTestWidget.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SerialTestWidget w;
    w.setWindowTitle("X5 串口验证工具 (serial_test)");
    w.resize(900, 620);
    w.show();
    return a.exec();
}
