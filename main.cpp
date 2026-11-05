#include <QApplication>
#include <QIcon>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FanqieUploader"));
    app.setApplicationDisplayName(QStringLiteral("番茄章节上传工具"));
    app.setOrganizationName(QStringLiteral("carson"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    MainWindow w;
    w.show();

    // 无人值守自测：CU_SELFTEST=1 CU_SHOT=截图路径 → 截图后自动退出
    if (!qEnvironmentVariable("CU_SELFTEST").isEmpty())
        w.runAutoTest(qEnvironmentVariable("CU_SHOT"));

    return app.exec();
}
