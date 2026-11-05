// CdpClient 多标签逻辑冒烟测试（不碰番茄账号）
//   1) 连上调试端口
//   2) 记下现有标签 id
//   3) 在页面里 window.open 一个含 /publish/ 的新标签
//   4) followNewTarget 应当接住它，并报出新的 target id
//   5) closeTarget 关掉它，再 fetchTargets 确认已消失
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include <cstdio>

#include "cdpclient.h"

static int g_fail = 0;
static void ok(bool cond, const QString &name, const QString &extra = QString())
{
    if (!cond)
        ++g_fail;
    printf("%s | %s%s\n", cond ? "PASS" : "FAIL", name.toUtf8().constData(),
           extra.isEmpty() ? "" : (QStringLiteral(" | ") + extra).toUtf8().constData());
    fflush(stdout);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const int port = argc > 1 ? QString::fromLocal8Bit(argv[1]).toInt() : 9444;

    CdpClient cdp;
    cdp.setPort(port);
    QObject::connect(&cdp, &CdpClient::log, [](const QString &m) {
        printf("[cdp] %s\n", m.toUtf8().constData());
        fflush(stdout);
    });

    QStringList before;
    QString openedId;

    // 测试用：连任意 http(s) 页面即可（生产代码里筛选 fanqienovel.com）
    cdp.ensureTarget([](const QString &u) { return u.startsWith("http"); },
                     [](bool, const QString &) {});
    QTimer::singleShot(1200, [&]() {
        if (!cdp.isConnected()) {
            printf("FAIL | 连不上调试端口 %d\n", port);
            fflush(stdout);
            app.exit(1);
            return;
        }
        cdp.fetchTargets([&](bool ok1, const QJsonArray &pages, const QString &why) {
            ok(ok1 && !pages.isEmpty(), QStringLiteral("读到现有标签"), why);
            for (const QJsonValue &v : pages)
                before << v.toObject().value(QStringLiteral("id")).toString();
            printf("  现有标签数：%d\n", int(before.size()));
            fflush(stdout);

            const QString js = QStringLiteral(
                "window.open('https://example.com/publish/tabtest-%1');'opened'")
                                   .arg(QDateTime::currentMSecsSinceEpoch());
            cdp.evaluate(js, [&](bool ok2, const QJsonValue &, const QString &e) {
                ok(ok2, QStringLiteral("window.open 新标签"), e);
                cdp.followNewTarget(before, QStringLiteral("/publish/"), 9000,
                                    [&](bool ok3, const QString &id, const QString &why3) {
                    ok(ok3 && !id.isEmpty() && !before.contains(id),
                       QStringLiteral("followNewTarget 接住新标签"), why3);
                    ok(cdp.currentUrl().contains(QStringLiteral("/publish/")),
                       QStringLiteral("当前连接已切到新标签"), cdp.currentUrl());
                    openedId = id;

                    cdp.evaluate(QStringLiteral("location.href"), [&](bool ok4, const QJsonValue &v4, const QString &) {
                        ok(ok4 && v4.toString().contains(QStringLiteral("/publish/")),
                           QStringLiteral("新标签里能执行 JS"), v4.toString());

                        cdp.closeTarget(openedId);
                        QTimer::singleShot(1500, [&]() {
                            cdp.fetchTargets([&](bool ok5, const QJsonArray &pages2, const QString &) {
                                bool still = false;
                                for (const QJsonValue &v : pages2)
                                    if (v.toObject().value(QStringLiteral("id")).toString() == openedId)
                                        still = true;
                                ok(ok5 && !still, QStringLiteral("closeTarget 关掉了标签"),
                                   still ? QStringLiteral("还在") : QString());
                                printf("合计失败 %d 项\n", g_fail);
                                fflush(stdout);
                                app.exit(g_fail ? 2 : 0);
                            });
                        });
                    });
                });
            });
        });
    });
    return app.exec();
}
