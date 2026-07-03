#include <QApplication>
#include "MainWindow/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("PA System");
    QApplication::setOrganizationName("NDT PA");

    MainWindow w;
    w.resize(1500, 900);
    w.show();
    return app.exec();
}
