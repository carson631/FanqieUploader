#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // 启动耗时剖析：CU_BOOT=1 时把分段耗时写进 %TEMP%/fqboot.txt 后退出
    QElapsedTimer boot;
    boot.start();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FanqieUploader"));
    app.setApplicationDisplayName(QStringLiteral("番茄章节上传工具"));
    app.setOrganizationName(QStringLiteral("carson"));

    // 基础字号统一在这里设一次。之前 QSS 里给 QLabel / QPushButton / 输入框
    // 都写了 font-size，每个控件建立时都要跑一次字体引擎匹配 —— 78 个控件
    // 累计近 500ms（polish 480ms → 4ms 的实测差距）。字体族沿用系统默认。
    {
        QFont f = app.font();
        f.setPixelSize(13);
        app.setFont(f);
    }
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    const qint64 tApp = boot.elapsed();

    MainWindow w;
    const qint64 tWin = boot.elapsed();
    w.show();
    const qint64 tShow = boot.elapsed();

    if (qEnvironmentVariableIsSet("CU_BOOT")) {
        // 首帧之后（0ms）与「可交互」（120ms）各记一次
        QTimer::singleShot(0, &w, [&boot, tApp, tWin, tShow]() {
            const qint64 tTick = boot.elapsed();
            QTimer::singleShot(120, [&boot, tApp, tWin, tShow, tTick]() {
                const qint64 tReady = boot.elapsed();
                const QString msg =
                    QStringLiteral("qapp=%1ms ui=%2ms show=%3ms firstTick=%4ms ready=%5ms")
                        .arg(tApp).arg(tWin - tApp).arg(tShow - tWin)
                        .arg(tTick - tShow).arg(tReady);
                QFile f(QDir::tempPath() + QStringLiteral("/fqboot.txt"));
                if (f.open(QIODevice::WriteOnly | QIODevice::Append))
                    f.write(msg.toUtf8());
                qApp->quit();
            });
        });
    }

    // 无人值守自测：CU_SELFTEST=1 CU_SHOT=截图路径 → 截图后自动退出
    // CU_RUN=1 则是真机联调：自动连接 + 自动点开始上传（配合 CU_DRY=1 只填表不发布）
    if (!qEnvironmentVariable("CU_SELFTEST").isEmpty()
        || !qEnvironmentVariable("CU_RUN").isEmpty())
        w.runAutoTest(qEnvironmentVariable("CU_SHOT"));

    return app.exec();
}
