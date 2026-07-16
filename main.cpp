#include <QApplication>
#include <QScreen>
#include "MainWindow/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("PA System");
    QApplication::setOrganizationName("NDT PA");

    MainWindow w;

    QScreen *screen = app.primaryScreen();
    QRect avail = screen->availableGeometry();

    // 先显示窗口，让系统创建标题栏和边框
    w.show();

    // 计算窗口标题栏/边框额外占用的尺寸
    int frameExtraW = w.frameGeometry().width()  - w.geometry().width();
    int frameExtraH = w.frameGeometry().height() - w.geometry().height();

    // 调整窗口内容区大小，使窗口整体（含边框）正好铺满可用区域
    w.move(avail.x(), avail.y());
    w.resize(avail.width() - frameExtraW, avail.height() - frameExtraH);

    return app.exec();
}
