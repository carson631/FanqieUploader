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
    // CU_RUN=1 则是真机联调：自动连接 + 自动点开始上传（配合 CU_DRY=1 只填表不发布）
    if (!qEnvironmentVariable("CU_SELFTEST").isEmpty()
        || !qEnvironmentVariable("CU_RUN").isEmpty())
        w.runAutoTest(qEnvironmentVariable("CU_SHOT"));

    return app.exec();
}
